// Copyright 2018 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ledger/TrustLineWrapper.h"
#include "ledger/LedgerState.h"
#include "ledger/LedgerStateHeader.h"
#include "transactions/TransactionUtils.h"
#include "util/XDROperators.h"
#include "util/types.h"

namespace stellar
{

// Declarations of TrustLineWrapper implementations ---------------------------
class TrustLineWrapper::NonIssuerImpl : public TrustLineWrapper::AbstractImpl
{
    LedgerStateEntry mEntry;

  public:
    NonIssuerImpl(LedgerStateEntry&& entry);

    operator bool() const override;

    AccountID const& getAccountID() const override;
    Asset const& getAsset() const override;

    int64_t getBalance() const override;
    bool addBalance(LedgerStateHeader const& header, int64_t delta) override;

    int64_t getDebt() const override;
    bool addDebt(LedgerStateHeader const& header, int64_t delta) override;

    int64_t getLimit() const override;

    int64_t getBuyingLiabilities(LedgerStateHeader const& header) override;
    int64_t getSellingLiabilities(LedgerStateHeader const& header) override;

    int64_t addBuyingLiabilities(LedgerStateHeader const& header, int64_t delta,
                                 bool isMarginTrade = false,
                                 int64_t calculatedMaxLiability = 0) override;
    int64_t addSellingLiabilities(LedgerStateHeader const& header,
                                  int64_t delta, bool isMarginTrade = false,
                                  int64_t calculatedMaxLiability = 0) override;

    bool isAuthorized() const override;

    bool isLiquidating() const override;

    bool isBaseAsset(AbstractLedgerState& ls) const override;

    int64_t getAvailableBalance(LedgerStateHeader const& header) const override;

    int64_t getMaxAmountReceive(LedgerStateHeader const& header) const override;
};

class TrustLineWrapper::IssuerImpl : public TrustLineWrapper::AbstractImpl
{
    AccountID const mAccountID;
    Asset const mAsset;

  public:
    IssuerImpl(AccountID const& accountID, Asset const& asset);

    operator bool() const override;

    AccountID const& getAccountID() const override;
    Asset const& getAsset() const override;

    int64_t getBalance() const override;
    bool addBalance(LedgerStateHeader const& header, int64_t delta) override;

    int64_t getDebt() const override;
    bool addDebt(LedgerStateHeader const& header, int64_t delta) override;

    int64_t getLimit() const override;

    int64_t getBuyingLiabilities(LedgerStateHeader const& header) override;
    int64_t getSellingLiabilities(LedgerStateHeader const& header) override;

    int64_t addBuyingLiabilities(LedgerStateHeader const& header, int64_t delta,
                                 bool isMarginTrade = false,
                                 int64_t calculatedMaxLiability = 0) override;
    int64_t addSellingLiabilities(LedgerStateHeader const& header,
                                  int64_t delta, bool isMarginTrade = false,
                                  int64_t calculatedMaxLiability = 0) override;

    bool isAuthorized() const override;

    bool isLiquidating() const override;

    bool isBaseAsset(AbstractLedgerState& ls) const override;

    int64_t getAvailableBalance(LedgerStateHeader const& header) const override;

    int64_t getMaxAmountReceive(LedgerStateHeader const& header) const override;
};

// Implementation of TrustLineWrapper -----------------------------------------
TrustLineWrapper::TrustLineWrapper()
{
}

TrustLineWrapper::TrustLineWrapper(AbstractLedgerState& ls,
                                   AccountID const& accountID,
                                   Asset const& asset)
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        throw std::runtime_error("trustline for native asset");
    }

    if (!(getIssuer(asset) == accountID))
    {
        LedgerKey key(TRUSTLINE);
        key.trustLine().accountID = accountID;
        key.trustLine().asset = asset;
        auto entry = ls.load(key);
        if (entry)
        {
            mImpl = std::make_unique<NonIssuerImpl>(std::move(entry));
        }
    }
    else
    {
        mImpl = std::make_unique<IssuerImpl>(accountID, asset);
    }
}

TrustLineWrapper::TrustLineWrapper(LedgerStateEntry&& entry)
{
    if (entry)
    {
        mImpl = std::make_unique<NonIssuerImpl>(std::move(entry));
    }
}

TrustLineWrapper::operator bool() const
{
    return (bool)mImpl && (bool)(*mImpl);
}

AccountID const&
TrustLineWrapper::getAccountID() const
{
    return getImpl()->getAccountID();
}

Asset const&
TrustLineWrapper::getAsset() const
{
    return getImpl()->getAsset();
}

int64_t
TrustLineWrapper::getBalance() const
{
    return getImpl()->getBalance();
}

int64_t
TrustLineWrapper::getDebt() const
{
    return getImpl()->getDebt();
}

int64_t
TrustLineWrapper::getLimit() const
{
    return getImpl()->getLimit();
}

bool
TrustLineWrapper::addBalance(LedgerStateHeader const& header, int64_t delta)
{
    return getImpl()->addBalance(header, delta);
}

bool
TrustLineWrapper::addDebt(LedgerStateHeader const& header, int64_t delta)
{
    return getImpl()->addDebt(header, delta);
}

int64_t
TrustLineWrapper::getBuyingLiabilities(LedgerStateHeader const& header)
{
    return getImpl()->getBuyingLiabilities(header);
}

int64_t
TrustLineWrapper::getSellingLiabilities(LedgerStateHeader const& header)
{
    return getImpl()->getSellingLiabilities(header);
}

int64_t
TrustLineWrapper::addBuyingLiabilities(LedgerStateHeader const& header,
                                       int64_t delta, bool isMarginTrade,
                                       int64_t calculatedMaxLiability)
{
    return getImpl()->addBuyingLiabilities(header, delta, isMarginTrade,
                                           calculatedMaxLiability);
}

int64_t
TrustLineWrapper::addSellingLiabilities(LedgerStateHeader const& header,
                                        int64_t delta, bool isMarginTrade,
                                        int64_t calculatedMaxLiability)
{
    return getImpl()->addSellingLiabilities(header, delta, isMarginTrade,
                                            calculatedMaxLiability);
}

bool
TrustLineWrapper::isAuthorized() const
{
    return getImpl()->isAuthorized();
}

bool
TrustLineWrapper::isLiquidating() const
{
    return getImpl()->isLiquidating();
}

bool
TrustLineWrapper::isBaseAsset(AbstractLedgerState& ls) const
{
    return getImpl()->isBaseAsset(ls);
}

int64_t
TrustLineWrapper::getAvailableBalance(LedgerStateHeader const& header) const
{
    return getImpl()->getAvailableBalance(header);
}

int64_t
TrustLineWrapper::getMaxAmountReceive(LedgerStateHeader const& header) const
{
    return getImpl()->getMaxAmountReceive(header);
}

void
TrustLineWrapper::deactivate()
{
    mImpl.reset();
}

std::unique_ptr<TrustLineWrapper::AbstractImpl> const&
TrustLineWrapper::getImpl() const
{
    if (!(*this))
    {
        throw std::runtime_error("TrustLineWrapper not active");
    }
    return mImpl;
}

// Implementation of TrustLineWrapper::NonIssuerImpl --------------------------
TrustLineWrapper::NonIssuerImpl::NonIssuerImpl(LedgerStateEntry&& entry)
    : mEntry(std::move(entry))
{
}

TrustLineWrapper::NonIssuerImpl::operator bool() const
{
    return (bool)mEntry;
}

AccountID const&
TrustLineWrapper::NonIssuerImpl::getAccountID() const
{
    return mEntry.current().data.trustLine().accountID;
}

Asset const&
TrustLineWrapper::NonIssuerImpl::getAsset() const
{
    return mEntry.current().data.trustLine().asset;
}

int64_t
TrustLineWrapper::NonIssuerImpl::getBalance() const
{
    return mEntry.current().data.trustLine().balance;
}

int64_t
TrustLineWrapper::NonIssuerImpl::getDebt() const
{
    return mEntry.current().data.trustLine().debt;
}

int64_t
TrustLineWrapper::NonIssuerImpl::getLimit() const
{
    return mEntry.current().data.trustLine().limit;
}

bool
TrustLineWrapper::NonIssuerImpl::addBalance(LedgerStateHeader const& header,
                                            int64_t delta)
{
    return stellar::addBalance(header, mEntry, delta);
}

bool
TrustLineWrapper::NonIssuerImpl::addDebt(LedgerStateHeader const& header,
                                         int64_t delta)
{
    return stellar::addDebt(header, mEntry, delta);
}

int64_t
TrustLineWrapper::NonIssuerImpl::getBuyingLiabilities(
    LedgerStateHeader const& header)
{
    return stellar::getBuyingLiabilities(header, mEntry);
}

int64_t
TrustLineWrapper::NonIssuerImpl::getSellingLiabilities(
    LedgerStateHeader const& header)
{
    return stellar::getSellingLiabilities(header, mEntry);
}

int64_t
TrustLineWrapper::NonIssuerImpl::addBuyingLiabilities(
    LedgerStateHeader const& header, int64_t delta, bool isMarginTrade,
    int64_t calculatedMaxLiability)
{
    return stellar::addBuyingLiabilities(header, mEntry, delta, isMarginTrade,
                                         calculatedMaxLiability);
}

int64_t
TrustLineWrapper::NonIssuerImpl::addSellingLiabilities(
    LedgerStateHeader const& header, int64_t delta, bool isMarginTrade,
    int64_t calculatedMaxLiability)
{
    return stellar::addSellingLiabilities(header, mEntry, delta, isMarginTrade,
                                          calculatedMaxLiability);
}

bool
TrustLineWrapper::NonIssuerImpl::isAuthorized() const
{
    return stellar::isAuthorized(mEntry);
}

bool
TrustLineWrapper::NonIssuerImpl::isLiquidating() const
{
    return stellar::isLiquidating(mEntry);
}

bool
TrustLineWrapper::NonIssuerImpl::isBaseAsset(AbstractLedgerState& ls) const
{
    return stellar::isBaseAsset(ls, mEntry);
}

int64_t
TrustLineWrapper::NonIssuerImpl::getAvailableBalance(
    LedgerStateHeader const& header) const
{
    return stellar::getAvailableBalance(header, mEntry);
}

int64_t
TrustLineWrapper::NonIssuerImpl::getMaxAmountReceive(
    LedgerStateHeader const& header) const
{
    return stellar::getMaxAmountReceive(header, mEntry);
}

// Implementation of TrustLineWrapper::IssuerImpl -----------------------------
TrustLineWrapper::IssuerImpl::IssuerImpl(AccountID const& accountID,
                                         Asset const& asset)
    : mAccountID(accountID), mAsset(asset)
{
}

TrustLineWrapper::IssuerImpl::operator bool() const
{
    return true;
}

AccountID const&
TrustLineWrapper::IssuerImpl::getAccountID() const
{
    return mAccountID;
}

Asset const&
TrustLineWrapper::IssuerImpl::getAsset() const
{
    return mAsset;
}

int64_t
TrustLineWrapper::IssuerImpl::getBalance() const
{
    return INT64_MAX;
}

int64_t
TrustLineWrapper::IssuerImpl::getDebt() const
{
    return INT64_MAX;
}

int64_t
TrustLineWrapper::IssuerImpl::getLimit() const
{
    return INT64_MAX;
}

bool
TrustLineWrapper::IssuerImpl::addBalance(LedgerStateHeader const& header,
                                         int64_t delta)
{
    return true;
}

bool
TrustLineWrapper::IssuerImpl::addDebt(LedgerStateHeader const& header,
                                      int64_t delta)
{
    return true;
}

int64_t
TrustLineWrapper::IssuerImpl::getBuyingLiabilities(
    LedgerStateHeader const& header)
{
    return 0;
}

int64_t
TrustLineWrapper::IssuerImpl::getSellingLiabilities(
    LedgerStateHeader const& header)
{
    return 0;
}

int64_t
TrustLineWrapper::IssuerImpl::addBuyingLiabilities(
    LedgerStateHeader const& header, int64_t delta, bool isMarginTrade,
    int64_t calculatedMaxLiability)
{
    return true;
}

int64_t
TrustLineWrapper::IssuerImpl::addSellingLiabilities(
    LedgerStateHeader const& header, int64_t delta, bool isMarginTrade,
    int64_t calculatedMaxLiability)
{
    return true;
}

bool
TrustLineWrapper::IssuerImpl::isAuthorized() const
{
    return true;
}

bool
TrustLineWrapper::IssuerImpl::isLiquidating() const
{
    return false;
}

bool
TrustLineWrapper::IssuerImpl::isBaseAsset(AbstractLedgerState& ls) const
{
    return false;
}

int64_t
TrustLineWrapper::IssuerImpl::getAvailableBalance(
    LedgerStateHeader const& header) const
{
    return INT64_MAX;
}

int64_t
TrustLineWrapper::IssuerImpl::getMaxAmountReceive(
    LedgerStateHeader const& header) const
{
    return INT64_MAX;
}

// Declarations of ConstTrustLineWrapper implementations ----------------------
class ConstTrustLineWrapper::NonIssuerImpl
    : public ConstTrustLineWrapper::AbstractImpl
{
    ConstLedgerStateEntry mEntry;

  public:
    NonIssuerImpl(ConstLedgerStateEntry&& entry);

    operator bool() const override;

    int64_t getBalance() const override;

    int64_t getDebt() const override;

    int64_t getLimit() const override;

    bool isAuthorized() const override;

    bool isLiquidating() const override;

    bool isBaseAsset(AbstractLedgerState& ls) const override;

    int64_t getAvailableBalance(LedgerStateHeader const& header) const override;

    int64_t getMaxAmountReceive(LedgerStateHeader const& header) const override;

    Asset getAsset() const override;
};

class ConstTrustLineWrapper::IssuerImpl
    : public ConstTrustLineWrapper::AbstractImpl
{
  public:
    operator bool() const override;

    int64_t getBalance() const override;

    int64_t getDebt() const override;

    int64_t getLimit() const override;

    bool isAuthorized() const override;

    bool isLiquidating() const override;

    bool isBaseAsset(AbstractLedgerState& ls) const override;

    int64_t getAvailableBalance(LedgerStateHeader const& header) const override;

    int64_t getMaxAmountReceive(LedgerStateHeader const& header) const override;

    Asset getAsset() const override;
};

// Implementation of ConstTrustLineWrapper ------------------------------------
ConstTrustLineWrapper::ConstTrustLineWrapper()
{
}

ConstTrustLineWrapper::ConstTrustLineWrapper(AbstractLedgerState& ls,
                                             AccountID const& accountID,
                                             Asset const& asset)
{
    if (!(getIssuer(asset) == accountID))
    {
        LedgerKey key(TRUSTLINE);
        key.trustLine().accountID = accountID;
        key.trustLine().asset = asset;
        auto entry = ls.loadWithoutRecord(key);
        if (entry)
        {
            mImpl = std::make_unique<NonIssuerImpl>(std::move(entry));
        }
    }
    else
    {
        mImpl = std::make_unique<IssuerImpl>();
    }
}

ConstTrustLineWrapper::ConstTrustLineWrapper(ConstLedgerStateEntry&& entry)
{
    if (entry)
    {
        mImpl = std::make_unique<NonIssuerImpl>(std::move(entry));
    }
}

ConstTrustLineWrapper::operator bool() const
{
    return (bool)mImpl && (bool)(*mImpl);
}

Asset
ConstTrustLineWrapper::getAsset() const
{
    return getImpl()->getAsset();
}

int64_t
ConstTrustLineWrapper::getBalance() const
{
    return getImpl()->getBalance();
}

int64_t
ConstTrustLineWrapper::getDebt() const
{
    return getImpl()->getDebt();
}

int64_t
ConstTrustLineWrapper::getLimit() const
{
    return getImpl()->getLimit();
}

bool
ConstTrustLineWrapper::isAuthorized() const
{
    return getImpl()->isAuthorized();
}

bool
ConstTrustLineWrapper::isLiquidating() const
{
    return getImpl()->isLiquidating();
}

bool
ConstTrustLineWrapper::isBaseAsset(AbstractLedgerState& ls) const
{
    return getImpl()->isBaseAsset(ls);
}

int64_t
ConstTrustLineWrapper::getAvailableBalance(
    LedgerStateHeader const& header) const
{
    return getImpl()->getAvailableBalance(header);
}

int64_t
ConstTrustLineWrapper::getMaxAmountReceive(
    LedgerStateHeader const& header) const
{
    return getImpl()->getMaxAmountReceive(header);
}

std::unique_ptr<ConstTrustLineWrapper::AbstractImpl> const&
ConstTrustLineWrapper::getImpl() const
{
    if (!(*this))
    {
        throw std::runtime_error("ConstTrustLineWrapper not active");
    }
    return mImpl;
}

// Implementation of ConstTrustLineWrapper::NonIssuerImpl ---------------------
ConstTrustLineWrapper::NonIssuerImpl::NonIssuerImpl(
    ConstLedgerStateEntry&& entry)
    : mEntry(std::move(entry))
{
}

ConstTrustLineWrapper::NonIssuerImpl::operator bool() const
{
    return (bool)mEntry;
}

int64_t
ConstTrustLineWrapper::NonIssuerImpl::getBalance() const
{
    return mEntry.current().data.trustLine().balance;
}

int64_t
ConstTrustLineWrapper::NonIssuerImpl::getDebt() const
{
    return mEntry.current().data.trustLine().debt;
}

int64_t
ConstTrustLineWrapper::NonIssuerImpl::getLimit() const
{
    return mEntry.current().data.trustLine().limit;
}

bool
ConstTrustLineWrapper::NonIssuerImpl::isAuthorized() const
{
    return stellar::isAuthorized(mEntry);
}

bool
ConstTrustLineWrapper::NonIssuerImpl::isLiquidating() const
{
    return stellar::isLiquidating(mEntry);
}

bool
ConstTrustLineWrapper::NonIssuerImpl::isBaseAsset(AbstractLedgerState& ls) const
{
    return stellar::isBaseAsset(ls, mEntry);
}

int64_t
ConstTrustLineWrapper::NonIssuerImpl::getAvailableBalance(
    LedgerStateHeader const& header) const
{
    return stellar::getAvailableBalance(header, mEntry);
}

int64_t
ConstTrustLineWrapper::NonIssuerImpl::getMaxAmountReceive(
    LedgerStateHeader const& header) const
{
    return stellar::getMaxAmountReceive(header, mEntry);
}

Asset
ConstTrustLineWrapper::NonIssuerImpl::getAsset() const
{
    return mEntry.current().data.trustLine().asset;
}

// Implementation of ConstTrustLineWrapper::IssuerImpl ------------------------
ConstTrustLineWrapper::IssuerImpl::operator bool() const
{
    return true;
}

int64_t
ConstTrustLineWrapper::IssuerImpl::getBalance() const
{
    return INT64_MAX;
}

int64_t
ConstTrustLineWrapper::IssuerImpl::getDebt() const
{
    return 0;
}


int64_t
ConstTrustLineWrapper::IssuerImpl::getLimit() const
{
    return INT64_MAX;
}

bool
ConstTrustLineWrapper::IssuerImpl::isAuthorized() const
{
    return true;
}

bool
ConstTrustLineWrapper::IssuerImpl::isLiquidating() const
{
    return false;
}

bool
ConstTrustLineWrapper::IssuerImpl::isBaseAsset(AbstractLedgerState& ls) const
{
    return false;
}

int64_t
ConstTrustLineWrapper::IssuerImpl::getAvailableBalance(
    LedgerStateHeader const& header) const
{
    return INT64_MAX;
}

int64_t
ConstTrustLineWrapper::IssuerImpl::getMaxAmountReceive(
    LedgerStateHeader const& header) const
{
    return INT64_MAX;
}

Asset
ConstTrustLineWrapper::IssuerImpl::getAsset() const
{
    return Asset();
}
}
