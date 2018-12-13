#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"

namespace stellar
{

class AbstractLedgerState;

class ManageOfferOpFrame : public OperationFrame
{
    bool checkOfferValid(medida::MetricsRegistry& metrics,
                         AbstractLedgerState& lsOuter);

    bool computeOfferExchangeParameters(
        Application& app, AbstractLedgerState& lsOuter,
        LedgerEntry const& offer, bool creatingNewOffer, bool isMarginTrade,
        int64_t& maxSheepSend, int64_t& maxWheatReceive);

    int64_t computeMaximumSellAmount(AbstractLedgerState& lsOuter,
                                     LedgerStateHeader const& header,
                                     Asset const& sheep, Asset const& wheat,
                                     Price const& price);

    ManageOfferResult&
    innerResult()
    {
        return mResult.tr().manageOfferResult();
    }

    ManageOfferOp const& mManageOffer;

    OfferEntry buildOffer(AccountID const& account, ManageOfferOp const& op,
                          uint32 flags);

  protected:
    bool mPassive;
    bool mMarginTrade;

  public:
    ManageOfferOpFrame(Operation const& op, OperationResult& res,
                       TransactionFrame& parentTx);

    bool doApply(Application& app, AbstractLedgerState& lsOuter) override;
    bool doCheckValid(Application& app, uint32_t ledgerVersion) override;

    static ManageOfferResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().manageOfferResult().code();
    }

    const static int64 maxLeverage = 10;
};
}
