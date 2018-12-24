#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"

namespace stellar
{
class AbstractLedgerState;

class InflationOpFrame : public OperationFrame
{
    InflationResult&
    innerResult()
    {
        return mResult.tr().inflationResult();
    }

    ThresholdLevel getThresholdLevel() const override;

    bool getReferencePrice(AbstractLedgerState& lsouter, std::string feedName,
                           PublicKey& issuerKey, double& result);
    bool getMidOrderbookPrice(Application& app, Asset& coin1,
                              Asset& coin2, Asset& base, double& result);
    bool getAvgOfferPrice(Application& app, Asset& coin1,
                          Asset& coin2, Asset& base, double& result);

    std::string base64_decode(std::string const& encoded_string);

    std::string base64_encode(unsigned char const* bytes_to_encode,
                              unsigned int in_len);

  public:
    InflationOpFrame(Operation const& op, OperationResult& res,
                     TransactionFrame& parentTx);

    bool doApply(Application& app, AbstractLedgerState& ls) override;
    bool doCheckValid(Application& app, uint32_t ledgerVersion) override;

    static InflationResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().inflationResult().code();
    }
};
}
