#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/ManageOfferOpFrame.h"

namespace stellar
{
class ManageOfferMarginOpHolder
{
  public:
    ManageOfferMarginOpHolder(Operation const& op);
    Operation mCreateOp;
};

class CreateMarginOfferOpFrame : public ManageOfferMarginOpHolder,
                                 public ManageOfferOpFrame
{
  public:
    CreateMarginOfferOpFrame(Operation const& op, OperationResult& res,
                             TransactionFrame& parentTx);
};
}
