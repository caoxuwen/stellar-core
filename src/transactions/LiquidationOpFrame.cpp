// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/LiquidationOpFrame.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerState.h"
#include "ledger/LedgerStateEntry.h"
#include "ledger/LedgerStateHeader.h"
#include "ledger/TrustLineWrapper.h"
#include "main/Application.h"
#include "math.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "overlay/StellarXDR.h"
#include "transactions/CreateLiquidationOfferOpFrame.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/types.h"
#include "xdr/Stellar-ledger-entries.h"
#include <list>

// const uint64_t LIQUIDATION_INTERVAL = (60 * 5); // every hour
const uint64_t LIQUIDATION_INTERVAL = (1); // every hour
// TODO: change start time
const stellar::int64 BASE_CONVERSION = 10000000; // 10^7
const stellar::int64 DEPTH_THRESHOLD = 100 * BASE_CONVERSION;
const stellar::int64 PRICE_MULTIPLE = 10000;
const double DIFF_THRESHOLD = 0.005;
const double MAX_DIFF_THRESHOLD = 0.1;

namespace stellar
{
LiquidationOpFrame::LiquidationOpFrame(Operation const& op,
                                       OperationResult& res,
                                       TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
{
}

bool
LiquidationOpFrame::doApply(Application& app, AbstractLedgerState& ls)
{
    auto header = ls.loadHeader();
    auto& lh = header.current();

    uint64_t closeTime = lh.scpValue.closeTime;
    uint64_t lastTime = lh.lastLiquidation;
    CLOG(DEBUG, "Tx") << "time " << closeTime << " " << lastTime << " ";

    if (closeTime < lastTime + LIQUIDATION_INTERVAL)
    {
        app.getMetrics()
            .NewMeter({"op-liquidation", "failure", "not-time"}, "operation")
            .Mark();
        innerResult().code(LIQUIDATION_NOT_TIME);
        return false;
    }

    innerResult().code(LIQUIDATION_SUCCESS);
    lh.lastLiquidation = closeTime;

    for (auto const& tradingPair : app.getConfig().TRADING)
    {
        TradingConfiguration config = tradingPair.second;

        CLOG(DEBUG, "Tx") << config.mName;
        CLOG(DEBUG, "Tx") << config.mCoin1.mName << " "
                          << KeyUtils::toStrKey(config.mCoin1.mIssuerKey);
        CLOG(DEBUG, "Tx") << config.mCoin2.mName << " "
                          << KeyUtils::toStrKey(config.mCoin2.mIssuerKey);
        CLOG(DEBUG, "Tx") << config.mBaseAsset.mName << " "
                          << KeyUtils::toStrKey(config.mBaseAsset.mIssuerKey);
        CLOG(DEBUG, "Tx") << config.mReferenceFeed.mName << " "
                          << KeyUtils::toStrKey(
                                 config.mReferenceFeed.mIssuerKey);

        // TODO: replace config.mReferenceFeed with highest voted key
        // through a mechanism similar to inflation destination
        double refPrice;
        if (!getReferencePrice(ls, config.mReferenceFeed.mName,
                               config.mReferenceFeed.mIssuerKey, refPrice))
        {
            app.getMetrics()
                .NewMeter({"op-liquidation", "failure", "no-reference-price"},
                          "operation")
                .Mark();
            innerResult().code(LIQUIDATION_NO_REFERENCE_PRICE);
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

        // process trustlines that should fall into liquidation mode

        double price1 = 1.0, price2 = 1.0;
        if (compareAsset(coin1, base))
        {
            price2 = refPrice;
        }
        else if (compareAsset(coin2, base))
        {
            price1 = refPrice;
        }
        else
        {
            // TODO: altcoin perpetual case
        }

        CLOG(DEBUG, "Tx") << "coin1 " << config.mCoin1.mName << " " << price1
                          << " " << config.mCoin2.mName << " " << price2;

        {
            // mark trustlines that should be liquidated
            auto trustlines = stellar::loadTrustLinesShouldLiquidate(
                ls, coin1, price1, coin2, price2, base);

            // LedgerState lsinner(ls);

            for (auto& trustline : trustlines)
            {
                TrustLineEntry& tl = trustline.data.trustLine();
                CLOG(DEBUG, "Tx") << KeyUtils::toStrKey(tl.accountID) << " "
                                  << tl.balance << " " << tl.debt;

                LedgerKey key1(TRUSTLINE);
                key1.trustLine().accountID = tl.accountID;
                key1.trustLine().asset = coin1;
                auto trustLineEntry1 = ls.load(key1);

                LedgerKey key2(TRUSTLINE);
                key2.trustLine().accountID = tl.accountID;
                key2.trustLine().asset = coin2;
                auto trustLineEntry2 = ls.load(key2);

                if (!(tl.flags & LIQUIDATION_FLAG))
                {
                    // if haven't marked liquidation
                    setLiquidation(trustLineEntry1, true);
                    setLiquidation(trustLineEntry2, true);
                }

                auto tl1 = trustLineEntry1.current().data.trustLine();
                auto tl2 = trustLineEntry2.current().data.trustLine();

                Price price(PRICE_MULTIPLE, PRICE_MULTIPLE);
                // price = n / d
                try
                {
                    // n if coin1 is base as two decimal usually enough for coin
                    if (compareAsset(coin1, base))
                    {
                        price.n = bigDivide(
                            abs(tl2.debt - tl2.balance), PRICE_MULTIPLE,
                            abs(tl1.balance - tl1.debt), ROUND_DOWN);
                    }
                    else if (compareAsset(coin2, base))
                    {
                        price.d = bigDivide(
                            abs(tl1.balance - tl1.debt), PRICE_MULTIPLE,
                            abs(tl2.debt - tl2.balance), ROUND_DOWN);
                    }
                    else
                    {
                        // TODO: altcoin perpetual case
                    }
                }
                catch (...)
                {
                    // if overflowed, use ref price instead
                    if (compareAsset(coin1, base))
                    {
                        price.d =
                            (stellar::int32)floor(refPrice * PRICE_MULTIPLE);
                    }
                    else if (compareAsset(coin2, base))
                    {
                        price.n =
                            (stellar::int32)floor(refPrice * PRICE_MULTIPLE);
                    }
                    else
                    {
                        // TODO: altcoin perpetual case
                    }
                }

                // process liquidation accounts
                if (tl1.debt > 0)
                {
                    // either coin1 debt > 0 or coin2 debt > 0
                    // need to repay all debt
                    // tl.debt == coin1.debt
                    applyCreateLiquidationOffer(app, ls, lh, tl.accountID,
                                                coin2, coin1, price, -tl2.debt);
                }
                else if (tl2.debt > 0)
                {
                    std::swap(price.n, price.d);

                    applyCreateLiquidationOffer(app, ls, lh, tl.accountID,
                                                coin1, coin2, price, -tl1.debt);
                }
            }
            // lsinner.commit();
        }

        {
            // unmark trustlines that don't need to be liquidated
            auto trustlines = stellar::loadTrustLinesUnderLiquidation(
                ls, coin1, price1, coin2, price2, base, false);

            for (auto& trustline : trustlines)
            {
                TrustLineEntry& tl = trustline.data.trustLine();
                CLOG(DEBUG, "Tx") << KeyUtils::toStrKey(tl.accountID) << " "
                                  << tl.balance << " " << tl.debt;

                LedgerKey key1(TRUSTLINE);
                key1.trustLine().accountID = tl.accountID;
                key1.trustLine().asset = coin1;

                auto trustLineEntry1 = ls.load(key1);
                setLiquidation(trustLineEntry1, false);

                LedgerKey key2(TRUSTLINE);
                key2.trustLine().accountID = tl.accountID;
                key2.trustLine().asset = coin2;

                auto trustLineEntry2 = ls.load(key2);
                setLiquidation(trustLineEntry2, false);
            }
        }

        // auto& effects = innerResult().effects;
    }
    app.getMetrics()
        .NewMeter({"op-liquidation", "success", "apply"}, "operation")
        .Mark();
    return true;
}

Operation
LiquidationOpFrame::createLiquidationOffer(AccountID const& account,
                                           Asset const& selling,
                                           Asset const& buying,
                                           Price const& price, int64_t amount)
{
    Operation op;
    op.body.type(CREATE_LIQUIDATION_OFFER);
    op.body.createLiquidationOfferOp().amount = amount;
    op.body.createLiquidationOfferOp().selling = selling;
    op.body.createLiquidationOfferOp().buying = buying;
    op.body.createLiquidationOfferOp().price = price;
    op.body.createLiquidationOfferOp().offerID = 0;

    op.sourceAccount.activate() = account;

    return op;
}

Operation
LiquidationOpFrame::createCancelOffer(AccountID const& account, uint64 offerID,
                                      Asset const& selling, Asset const& buying,
                                      Price const& price)
{
    Operation op;
    op.body.type(CREATE_LIQUIDATION_OFFER);
    op.body.createLiquidationOfferOp().offerID = offerID;
    op.body.createLiquidationOfferOp().selling = selling;
    op.body.createLiquidationOfferOp().buying = buying;
    op.body.createLiquidationOfferOp().price = price;
    op.body.createLiquidationOfferOp().amount = 0;

    op.sourceAccount.activate() = account;

    return op;
}

bool
LiquidationOpFrame::applyCreateLiquidationOffer(
    Application& app, AbstractLedgerState& ls, LedgerHeader& lh,
    AccountID const& accountid, Asset const& selling, Asset const& buying,
    Price const& price, int64_t amount)
{
    // check if one qualified offer exist
    auto offers = ls.getOffersByAccountAndAsset(accountid, selling);
    bool hasQualifiedOffer = false;
    if (offers.size() == 1)
    {
        for (auto const& x : offers)
        {
            LedgerEntry le = x.second;
            if (compareAsset(le.data.offer().selling, selling) &&
                compareAsset(le.data.offer().buying, buying) &&
                le.data.offer().amount == amount &&
                le.data.offer().price == price)
                hasQualifiedOffer = true;
        }
    }

    if (hasQualifiedOffer)
    {
        return true;
    }

    // no qualified offer, cancel all exisitng offers
    for (auto const& x : offers)
    {
        LedgerEntry le = x.second;

        OperationResult result;
        result.code(opINNER);
        result.tr().type(MANAGE_OFFER);

        auto op = createCancelOffer(
            accountid, le.data.offer().offerID, le.data.offer().selling,
            le.data.offer().buying, le.data.offer().price);
        CreateLiquidationOfferOpFrame frame(op, result, mParentTx);

        if (!frame.doCheckValid(app, lh.ledgerVersion) ||
            !frame.doApply(app, ls))
        {
            if (frame.getResultCode() != opINNER)
            {
                throw std::runtime_error(
                    "Unexpected error code from liquidation process");
            }
        }
    }

    auto op = createLiquidationOffer(accountid, selling, buying, price, amount);
    OperationResult result;
    result.code(opINNER);
    result.tr().type(MANAGE_OFFER);

    CreateLiquidationOfferOpFrame frame(op, result, mParentTx);
    if (!frame.doCheckValid(app, lh.ledgerVersion) || !frame.doApply(app, ls))
    {
        if (frame.getResultCode() != opINNER)
        {
            throw std::runtime_error(
                "Unexpected error code from liquidation process");
        }
    }

    return true;
}

TransactionFramePtr
LiquidationOpFrame::transactionFromOperations(Application& app,
                                              SecretKey const& from,
                                              SequenceNumber seq,
                                              const std::vector<Operation>& ops)
{
    auto e = TransactionEnvelope{};
    e.tx.sourceAccount = from.getPublicKey();
    e.tx.fee = static_cast<uint32_t>(
        (ops.size() * app.getLedgerManager().getLastTxFee()) & UINT32_MAX);
    e.tx.seqNum = seq;
    std::copy(std::begin(ops), std::end(ops),
              std::back_inserter(e.tx.operations));

    auto res = TransactionFrame::makeTransactionFromWire(app.getNetworkID(), e);
    res->addSignature(from);
    return res;
}

bool
LiquidationOpFrame::doCheckValid(Application& app, uint32_t ledgerVersion)
{
    return true;
}

ThresholdLevel
LiquidationOpFrame::getThresholdLevel() const
{
    return ThresholdLevel::LOW;
}
}