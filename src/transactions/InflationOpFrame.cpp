// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/InflationOpFrame.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerState.h"
#include "ledger/LedgerStateEntry.h"
#include "ledger/LedgerStateHeader.h"
#include "main/Application.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "overlay/StellarXDR.h"
#include "transactions/TransactionUtils.h"
#include "util/Decoder.h"
#include "util/Logging.h"
#include "util/types.h"
#include "xdr/Stellar-ledger-entries.h"
#include <list>

const uint32_t INFLATION_FREQUENCY = (60 * 60); // every hour
// inflation is .000190721 per 7 days, or 1% a year
// const int64_t INFLATION_RATE_TRILLIONTHS = 190721000LL;
// const int64_t TRILLION = 1000000000000LL;
// const int64_t INFLATION_WIN_MIN_PERCENT = 500000000LL; // .05%
// const int INFLATION_NUM_WINNERS = 2000;
const time_t INFLATION_START_TIME = (1404172800LL); // 1-jul-2014 (unix epoch)
// TODO: change start time
const stellar::int64 BASE_CONVERSION = 10000000; // 10^7
const stellar::int64 DEPTH_THRESHOLD = 100 * BASE_CONVERSION;

namespace stellar
{
InflationOpFrame::InflationOpFrame(Operation const& op, OperationResult& res,
                                   TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
{
}

bool
InflationOpFrame::doApply(Application& app, AbstractLedgerState& ls)
{
    // app.getConfig().NODE_SEED
    auto header = ls.loadHeader();
    auto& lh = header.current();

    time_t closeTime = lh.scpValue.closeTime;
    uint64_t seq = lh.inflationSeq;

    time_t inflationTime = (INFLATION_START_TIME + seq * INFLATION_FREQUENCY);
    if (closeTime < inflationTime)
    {
        app.getMetrics()
            .NewMeter({"op-inflation", "failure", "not-time"}, "operation")
            .Mark();
        innerResult().code(INFLATION_NOT_TIME);
        return false;
    }

    const std::string key = "ETHI_USDI";
    // TODO: check no find
    TradingConfiguration config = app.getConfig().TRADING.find(key)->second;

    /*
    CLOG(DEBUG, "Tx") << config.mName;
    CLOG(DEBUG, "Tx") << config.mCoin1[0];
    CLOG(DEBUG, "Tx") << config.mCoin2[0];
    CLOG(DEBUG, "Tx") << config.mBaseAsset[0];
    CLOG(DEBUG, "Tx") << config.mReferenceFeed[0];*/

    CLOG(DEBUG, "Tx") << config.mName;
    CLOG(DEBUG, "Tx") << config.mCoin1.mName << " "
                      << KeyUtils::toStrKey(config.mCoin1.mIssuerKey);
    CLOG(DEBUG, "Tx") << config.mCoin2.mName << " "
                      << KeyUtils::toStrKey(config.mCoin2.mIssuerKey);
    CLOG(DEBUG, "Tx") << config.mBaseAsset.mName << " "
                      << KeyUtils::toStrKey(config.mBaseAsset.mIssuerKey);
    CLOG(DEBUG, "Tx") << config.mReferenceFeed.mName << " "
                      << KeyUtils::toStrKey(config.mReferenceFeed.mIssuerKey);

    double refPrice;
    if (!getReferencePrice(ls, config.mReferenceFeed.mName,
                           config.mReferenceFeed.mIssuerKey, refPrice))
    {
        app.getMetrics()
            .NewMeter({"op-inflation", "failure", "no-reference-price"},
                      "operation")
            .Mark();
        innerResult().code(INFLATION_NO_REFERENCE_PRICE);
        return false;
    };
    CLOG(DEBUG, "Tx") << "refPrice " << refPrice;

    Asset coin1;
    coin1.type(ASSET_TYPE_CREDIT_ALPHANUM4);
    coin1.alphaNum4().issuer = config.mCoin1.mIssuerKey;
    strToAssetCode(coin1.alphaNum4().assetCode, config.mCoin1.mName);

    Asset coin2;
    coin2.type(ASSET_TYPE_CREDIT_ALPHANUM4);
    coin2.alphaNum4().issuer = config.mCoin2.mIssuerKey;
    strToAssetCode(coin2.alphaNum4().assetCode, config.mCoin2.mName);

    Asset base;
    base.type(ASSET_TYPE_CREDIT_ALPHANUM4);
    base.alphaNum4().issuer = config.mBaseAsset.mIssuerKey;
    strToAssetCode(base.alphaNum4().assetCode, config.mBaseAsset.mName);

    double midOrderbookPrice;
    if (!getMidOrderbookPrice(ls, coin1, coin2, base, midOrderbookPrice))
    {
        app.getMetrics()
            .NewMeter({"op-inflation", "failure", "invalid-mid-price"},
                      "operation")
            .Mark();
        innerResult().code(INFLATION_INVALID_MID_PRICE);
        return false;
    };

    /*
    Inflation is calculated using the following

    1. calculate tally of votes based on "inflationDest" set on each account
    2. take the top accounts (by vote) that get at least .05% of the vote
    3. If no accounts are over this threshold then the extra goes back to
    the inflation pool
    */

    /*
        int64_t totalVotes = lh.totalCoins;
        int64_t minBalance =
            bigDivide(totalVotes, INFLATION_WIN_MIN_PERCENT, TRILLION,
       ROUND_DOWN);

        auto winners = ls.queryInflationWinners(INFLATION_NUM_WINNERS,
       minBalance);

        auto inflationAmount = bigDivide(lh.totalCoins,
       INFLATION_RATE_TRILLIONTHS, TRILLION, ROUND_DOWN); auto amountToDole
       = inflationAmount + lh.feePool;

        lh.feePool = 0;*/
    lh.inflationSeq++;

    // now credit each account
    innerResult().code(INFLATION_SUCCESS);
    auto& payouts = innerResult().payouts();

    /*
        int64 leftAfterDole = amountToDole;

        for (auto const& w : winners)
        {
            int64_t toDoleThisWinner =
                bigDivide(amountToDole, w.votes, totalVotes, ROUND_DOWN);
            if (toDoleThisWinner == 0)
                continue;

            if (lh.ledgerVersion >= 10)
            {
                auto winner = stellar::loadAccountWithoutRecord(ls,
       w.accountID); if (winner)
                {
                    toDoleThisWinner = std::min(getMaxAmountReceive(header,
       winner), toDoleThisWinner); if (toDoleThisWinner == 0) continue;
                }
            }

            auto winner = stellar::loadAccount(ls, w.accountID);
            if (winner)
            {
                leftAfterDole -= toDoleThisWinner;
                if (lh.ledgerVersion <= 7)
                {
                    lh.totalCoins += toDoleThisWinner;
                }
                if (!addBalance(header, winner, toDoleThisWinner))
                {
                    throw std::runtime_error(
                        "inflation overflowed destination balance");
                }
                payouts.emplace_back(w.accountID, toDoleThisWinner);
            }
        }

        // put back in fee pool as unclaimed funds
        lh.feePool += leftAfterDole;
        if (lh.ledgerVersion > 7)
        {
            lh.totalCoins += inflationAmount;
        }
    */
    app.getMetrics()
        .NewMeter({"op-inflation", "success", "apply"}, "operation")
        .Mark();
    return true;
}

bool
InflationOpFrame::doCheckValid(Application& app, uint32_t ledgerVersion)
{
    return true;
}

ThresholdLevel
InflationOpFrame::getThresholdLevel() const
{
    return ThresholdLevel::LOW;
}

bool
InflationOpFrame::getReferencePrice(AbstractLedgerState& lsouter,
                                    std::string feedName, PublicKey& issuerKey,
                                    double& result)
{
    LedgerState ls(lsouter);
    auto data = stellar::loadData(ls, issuerKey, feedName);
    if (data)
    {
        try
        {
            DataValue val = data.current().data.data().dataValue;
            std::string base64_str = decoder::encode_b64(val);
            std::string res_str = base64_decode(base64_str);
            // CLOG(DEBUG, "Tx") << base64_str << " " << res_str;

            result = std::stod(res_str);
            return result;
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}

bool
InflationOpFrame::getMidOrderbookPrice(AbstractLedgerState& ls,
                                       Asset const& coin1, Asset const& coin2,
                                       Asset const& base, double& result)
{
    double bidprice = -1;
    if (!getAvgOfferPrice(ls, coin1, coin2, base, bidprice))
    {
        return false;
    }

    double offerprice = -1;
    if (!getAvgOfferPrice(ls, coin2, coin1, base, offerprice))
    {
        return false;
    }

    if (bidprice < 0 || offerprice < 0)
    {
        return false;
    }

    result = (bidprice + offerprice) / 2.0;

    return true;
}

bool
InflationOpFrame::getAvgOfferPrice(AbstractLedgerState& lsouter,
                                   Asset const& coin1, Asset const& coin2,
                                   Asset const& base, double& result)
{
    LedgerState ls(lsouter);

    bool coin1IsBase = false;
    if (compareAsset(coin1, base))
    {
        coin1IsBase = true;
    }
    else if (compareAsset(coin2, base))
    {
        coin1IsBase = false;
    }
    else
    {
        return false;
    }

    // assets are denominated in base
    std::set<LedgerKey> excludes;
    int64 depth = DEPTH_THRESHOLD;
    int64 total = 0;

    while (depth > 0)
    {
        std::list<LedgerEntry>::const_iterator iter;
        auto le = ls.getBestOffer(coin1, coin2, excludes);
        if (le)
        {

            auto entry = ls.load(LedgerEntryKey(*le));
            Price price = entry.current().data.offer().price;
            int64 amount = entry.current().data.offer().amount;
            int64 denominated_amount =
                coin1IsBase ? bigDivide(amount, price.n, price.d, ROUND_DOWN)
                            : amount;
            CLOG(DEBUG, "Tx") << "amount1 " << amount << " " << price.n << " "
                              << price.d << " " << coin1IsBase;

            int64 indexed_amount =
                depth < denominated_amount ? depth : denominated_amount;
            CLOG(DEBUG, "Tx")
                << "amount " << denominated_amount << " " << indexed_amount;

            if (coin1IsBase)
                total +=
                    bigDivide(indexed_amount, price.d, price.n, ROUND_DOWN);
            else
                total +=
                    bigDivide(indexed_amount, price.n, price.d, ROUND_DOWN);

            depth -= indexed_amount;

            excludes.insert(LedgerEntryKey(*le));
        }
        else
        {
            break;
        }
    }
    if (depth == DEPTH_THRESHOLD)
    {
        return false;
    }

    result = (double)total / (double)(DEPTH_THRESHOLD - depth);

    return true;
}

static bool
double_equals(double a, double b, double epsilon = __DBL_EPSILON__)
{
    return std::abs(a - b) < epsilon;
}

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

static inline bool
is_base64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string
InflationOpFrame::base64_encode(unsigned char const* bytes_to_encode,
                                unsigned int in_len)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
            ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] =
            ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::string
InflationOpFrame::base64_decode(std::string const& encoded_string)
{
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') &&
           is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] =
                (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
                              ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] =
            (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] =
            ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++)
            ret += char_array_3[j];
    }

    return ret;
}
}