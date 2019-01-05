#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"
#include "transactions/TransactionFrame.h"

namespace stellar
{
class AbstractLedgerState;

class LiquidationOpFrame : public OperationFrame
{
    LiquidationResult&
    innerResult()
    {
        return mResult.tr().liquidationResult();
    }

    ThresholdLevel getThresholdLevel() const override;

    Operation createLiquidationOffer(AccountID const& account,
                                     Asset const& selling, Asset const& buying,
                                     Price const& price, int64_t amount);
    Operation createCancelOffer(AccountID const& account, uint64 offerID,
                                Asset const& selling, Asset const& buying,
                                Price const& price);

    TransactionFramePtr
    transactionFromOperations(Application& app, SecretKey const& from,
                              SequenceNumber seq,
                              const std::vector<Operation>& ops);

    bool applyCreateLiquidationOffer(Application& app, AbstractLedgerState& ls,
                                     LedgerHeader& lh,
                                     AccountID const& accountid,
                                     Asset const& selling, Asset const& buying,
                                     Price const& price, int64_t amount,
                                     bool justCancel);

  public:
    LiquidationOpFrame(Operation const& op, OperationResult& res,
                       TransactionFrame& parentTx);

    bool doApply(Application& app, AbstractLedgerState& ls) override;
    bool doCheckValid(Application& app, uint32_t ledgerVersion) override;

    static LiquidationResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().liquidationResult().code();
    }
};
}
