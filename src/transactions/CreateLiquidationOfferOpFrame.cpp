// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/CreateLiquidationOfferOpFrame.h"

namespace stellar
{

// change from CreateLiquidationOfferOp to ManageOfferOp
ManageOfferLiquidationOpHolder::ManageOfferLiquidationOpHolder(Operation const& op)
{
    mCreateOp.body.type(MANAGE_OFFER);
    auto& manageOffer = mCreateOp.body.manageOfferOp();
    auto const& createLiquidationOp = op.body.createLiquidationOfferOp();
    manageOffer.amount = createLiquidationOp.amount;
    manageOffer.buying = createLiquidationOp.buying;
    manageOffer.selling = createLiquidationOp.selling;
    manageOffer.offerID = createLiquidationOp.offerID;
    manageOffer.price = createLiquidationOp.price;
    mCreateOp.sourceAccount = op.sourceAccount;
}

CreateLiquidationOfferOpFrame::CreateLiquidationOfferOpFrame(Operation const& op,
                                                     OperationResult& res,
                                                     TransactionFrame& parentTx)
    : ManageOfferLiquidationOpHolder(op), ManageOfferOpFrame(mCreateOp, res, parentTx)
{
    mMarginTrade = true; 
    mLiquidation = true;
}
}
