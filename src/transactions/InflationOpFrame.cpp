// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/InflationOpFrame.h"
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
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/types.h"
#include "xdr/Stellar-ledger-entries.h"
#include <list>

const uint64_t FUNDING_INTERVAL = (60 * 60);     // every hour
const stellar::int64 BASE_CONVERSION = 10000000; // 10^7
const stellar::int64 DEPTH_THRESHOLD = 100 * BASE_CONVERSION;
const double DIFF_THRESHOLD = 0.005;
const double MAX_DIFF_THRESHOLD = 0.1;

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
    auto header = ls.loadHeader();
    auto& lh = header.current();

    uint64_t closeTime = lh.scpValue.closeTime;
    uint64_t lastTime = lh.lastFunding;
    CLOG(DEBUG, "Tx") << "time " << closeTime << " " << lastTime << " ";

    if (closeTime < lastTime + FUNDING_INTERVAL)
    {
        app.getMetrics()
            .NewMeter({"op-inflation", "failure", "not-time"}, "operation")
            .Mark();
        innerResult().code(INFLATION_NOT_TIME);
        return false;
    }

    innerResult().code(INFLATION_SUCCESS);
    lh.inflationSeq++;
    lh.lastFunding = closeTime;
    
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
        if (!getMidOrderbookPrice(ls, coin1, coin2, base, midOrderbookPrice,
                                  DEPTH_THRESHOLD))
        {
            app.getMetrics()
                .NewMeter({"op-inflation", "failure", "invalid-mid-price"},
                          "operation")
                .Mark();
            innerResult().code(INFLATION_INVALID_MID_PRICE);
            return false;
        };

        CLOG(DEBUG, "Tx") << "midPrice " << midOrderbookPrice;

        // now credit each account
        auto& payouts = innerResult().payouts();

        if (fabs(midOrderbookPrice - refPrice) >= refPrice * DIFF_THRESHOLD)
        {
            // only doing funding mechanism if diff >= 0.05%
            double dratio = std::max<double>(
                -MAX_DIFF_THRESHOLD,
                std::min<double>((midOrderbookPrice - refPrice) / refPrice,
                                 MAX_DIFF_THRESHOLD));

            CLOG(DEBUG, "Tx") << "ref price " << refPrice << " mid price "
                              << midOrderbookPrice << " ratio " << dratio;

            // if ion > spot, shift collateral from longs to shorts
            // which entails +debt => +balance
            // Conversely, if ion < spot, shift collateral from shorts to longs
            // which entails +debt => -balance

            if (compareAsset(coin1, base) || compareAsset(coin2, base))
            {
                LedgerState lsinner(ls);
                // have to calculate through non-base asset
                // as its sum(debt) == 0 => sum(funding) == 0
                Asset nonbase = compareAsset(coin1, base) ? coin2 : coin1;

                auto debt = stellar::loadTrustLinesWithDebt(lsinner, nonbase);
                int64 debt_total = 0;
                for (auto& debtline : debt)
                {
                    TrustLineEntry& tl = debtline.data.trustLine();
                    CLOG(DEBUG, "Tx") << KeyUtils::toStrKey(tl.accountID) << " "
                                      << tl.balance << " " << tl.debt;
                    debt_total += tl.debt;

                    // negative because we used nonbase to compute
                    int64 delta = -tl.debt * dratio / refPrice;
                    CLOG(DEBUG, "Tx")
                        << KeyUtils::toStrKey(tl.accountID) << " " << delta;

                    auto stateentry =
                        stellar::loadTrustLine(lsinner, tl.accountID, base);
                    if (!stateentry.addBalance(lsinner.loadHeader(), delta))
                    {
                        throw std::runtime_error(
                            "funding overflowed entry limit");
                    }
                    payouts.emplace_back(tl.accountID, base, delta);
                }

                // conservation of collateral
                if (debt_total != 0)
                {
                    app.getMetrics()
                        .NewMeter({"op-inflation", "failure", "debt-not-zero"},
                                  "operation")
                        .Mark();
                    innerResult().code(INFLATION_DEBT_NOT_ZERO);
                    return false;
                }

                // if there's a mistake, not change is committed
                lsinner.commit();
            }
            else
            {
                // TODO: take care of altcoin cases
            }
        }
    }

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
}