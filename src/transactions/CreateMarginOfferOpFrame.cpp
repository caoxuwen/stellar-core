// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/CreateMarginOfferOpFrame.h"

namespace stellar
{

// change from CreateMarginOfferOp to ManageOfferOp
ManageOfferMarginOpHolder::ManageOfferMarginOpHolder(Operation const& op)
{
    mCreateOp.body.type(MANAGE_OFFER);
    auto& manageOffer = mCreateOp.body.manageOfferOp();
    auto const& createMarginOp = op.body.createMarginOfferOp();
    manageOffer.amount = createMarginOp.amount;
    manageOffer.buying = createMarginOp.buying;
    manageOffer.selling = createMarginOp.selling;
    manageOffer.offerID = 0;
    manageOffer.price = createMarginOp.price;
    mCreateOp.sourceAccount = op.sourceAccount;
}

CreateMarginOfferOpFrame::CreateMarginOfferOpFrame(Operation const& op,
                                                     OperationResult& res,
                                                     TransactionFrame& parentTx)
    : ManageOfferMarginOpHolder(op), ManageOfferOpFrame(mCreateOp, res, parentTx)
{
    mMarginTrade = true;
}
}
