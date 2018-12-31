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
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/types.h"
#include "xdr/Stellar-ledger-entries.h"
#include <list>

const uint64_t LIQUIDATION_INTERVAL = (60 * 5);     // every hour
const time_t INFLATION_START_TIME = (1404172800LL); // 1-jul-2014 (unix epoch)
// TODO: change start time
const stellar::int64 BASE_CONVERSION = 10000000; // 10^7
const stellar::int64 DEPTH_THRESHOLD = 100 * BASE_CONVERSION;
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

    for (auto const& tradingPair : app.getConfig().TRADING)
    {
        TradingConfiguration config = tradingPair.second;

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
    }

    innerResult().code(LIQUIDATION_SUCCESS);
    lh.lastLiquidation = closeTime;

    // now credit each account
    auto& effects = innerResult().effects;

    app.getMetrics()
        .NewMeter({"op-liquidation", "success", "apply"}, "operation")
        .Mark();
    return true;
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