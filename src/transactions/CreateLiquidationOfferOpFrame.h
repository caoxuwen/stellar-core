#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/ManageOfferOpFrame.h"

namespace stellar
{
class ManageOfferLiquidationOpHolder
{
  public:
    ManageOfferLiquidationOpHolder(Operation const& op);
    Operation mCreateOp;
};

class CreateLiquidationOfferOpFrame : public ManageOfferLiquidationOpHolder,
                                 public ManageOfferOpFrame
{
  public:
    CreateLiquidationOfferOpFrame(Operation const& op, OperationResult& res,
                             TransactionFrame& parentTx);
};
}
