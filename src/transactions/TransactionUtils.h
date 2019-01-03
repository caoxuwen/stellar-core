#pragma once

// Copyright 2018 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "xdr/Stellar-ledger-entries.h"

namespace stellar
{

class ConstLedgerStateEntry;
class ConstTrustLineWrapper;
class AbstractLedgerState;
class LedgerStateEntry;
class LedgerStateHeader;
class TrustLineWrapper;

LedgerStateEntry loadAccount(AbstractLedgerState& ls,
                             AccountID const& accountID);

ConstLedgerStateEntry loadAccountWithoutRecord(AbstractLedgerState& ls,
                                               AccountID const& accountID);

LedgerStateEntry loadData(AbstractLedgerState& ls, AccountID const& accountID,
                          std::string const& dataName);

LedgerStateEntry loadOffer(AbstractLedgerState& ls, AccountID const& sellerID,
                           uint64_t offerID);

TrustLineWrapper loadTrustLine(AbstractLedgerState& ls,
                               AccountID const& accountID, Asset const& asset);

ConstTrustLineWrapper loadTrustLineWithoutRecord(AbstractLedgerState& ls,
                                                 AccountID const& accountID,
                                                 Asset const& asset);

TrustLineWrapper loadTrustLineIfNotNative(AbstractLedgerState& ls,
                                          AccountID const& accountID,
                                          Asset const& asset);

ConstTrustLineWrapper loadTrustLineWithoutRecordIfNotNative(
    AbstractLedgerState& ls, AccountID const& accountID, Asset const& asset);

Asset makeDebtAsset();

std::vector<LedgerEntry> loadTrustLinesWithDebt(AbstractLedgerState& ls,
                                                Asset const& asset);
std::vector<LedgerEntry>
loadTrustLinesShouldLiquidate(AbstractLedgerState& ls, Asset const& asset1,
                              double ratio1, Asset const& asset2, double ratio2,
                              Asset const& assetBalance);

std::vector<LedgerEntry>
loadTrustLinesUnderLiquidation(AbstractLedgerState& ls, Asset const& asset1,
                               double ratio1, Asset const& asset2,
                               double ratio2, Asset const& assetBalance,
                               bool stillEligible);

bool isDebtAsset(Asset asset);

void acquireLiabilities(AbstractLedgerState& ls,
                        LedgerStateHeader const& header,
                        LedgerStateEntry const& offer,
                        bool isMarginTrade = false,
                        int64_t calculatedMaxLiability = 0);

bool addBalance(LedgerStateHeader const& header, LedgerStateEntry& entry,
                int64_t delta);

bool addDebt(LedgerStateHeader const& header, LedgerStateEntry& entry,
             int64_t delta);

bool addBuyingLiabilities(LedgerStateHeader const& header,
                          LedgerStateEntry& entry, int64_t delta,
                          bool isMarginTrade = false,
                          int64_t calculatedMaxLiability = 0);

bool addNumEntries(LedgerStateHeader const& header, LedgerStateEntry& entry,
                   int count);

bool addSellingLiabilities(LedgerStateHeader const& header,
                           LedgerStateEntry& entry, int64_t delta,
                           bool isMarginTrade = false,
                           int64_t calculatedMaxLiability = 0);

uint64_t generateID(LedgerStateHeader& header);

int64_t getAvailableBalance(LedgerStateHeader const& header,
                            LedgerEntry const& le);
int64_t getAvailableBalance(LedgerStateHeader const& header,
                            LedgerStateEntry const& entry);
int64_t getAvailableBalance(LedgerStateHeader const& header,
                            ConstLedgerStateEntry const& entry);

int64_t getBuyingLiabilities(LedgerStateHeader const& header,
                             LedgerEntry const& le);
int64_t getBuyingLiabilities(LedgerStateHeader const& header,
                             LedgerStateEntry const& offer);

int64_t getMaxAmountReceive(LedgerStateHeader const& header,
                            LedgerEntry const& le);
int64_t getMaxAmountReceive(LedgerStateHeader const& header,
                            LedgerStateEntry const& entry);
int64_t getMaxAmountReceive(LedgerStateHeader const& header,
                            ConstLedgerStateEntry const& entry);

int64_t getMinBalance(LedgerStateHeader const& header, uint32_t ownerCount);

int64_t getMinimumLimit(LedgerStateHeader const& header, LedgerEntry const& le);
int64_t getMinimumLimit(LedgerStateHeader const& header,
                        LedgerStateEntry const& entry);
int64_t getMinimumLimit(LedgerStateHeader const& header,
                        ConstLedgerStateEntry const& entry);

int64_t getOfferBuyingLiabilities(LedgerStateHeader const& header,
                                  LedgerEntry const& entry);
int64_t getOfferBuyingLiabilities(LedgerStateHeader const& header,
                                  LedgerStateEntry const& entry);

int64_t getOfferSellingLiabilities(LedgerStateHeader const& header,
                                   LedgerEntry const& entry);
int64_t getOfferSellingLiabilities(LedgerStateHeader const& header,
                                   LedgerStateEntry const& entry);

int64_t getSellingLiabilities(LedgerStateHeader const& header,
                              LedgerEntry const& le);
int64_t getSellingLiabilities(LedgerStateHeader const& header,
                              LedgerStateEntry const& offer);

uint64_t getStartingSequenceNumber(LedgerStateHeader const& header);

bool getReferencePrice(AbstractLedgerState& lsouter, std::string feedName,
                       PublicKey& issuerKey, double& result);
bool getMidOrderbookPrice(AbstractLedgerState& ls, Asset const& coin1,
                          Asset const& coin2, Asset const& base, double& result,
                          int64 DEPTH_THRESHOLD);
bool getAvgOfferPrice(AbstractLedgerState& ls, Asset const& coin1,
                      Asset const& coin2, Asset const& base, double& result,
                      int64 DEPTH_THRESHOLD);

std::string base64_decode(std::string const& encoded_string);

std::string base64_encode(unsigned char const* bytes_to_encode,
                          unsigned int in_len);

bool isAuthorized(LedgerEntry const& le);
bool isAuthorized(LedgerStateEntry const& entry);
bool isAuthorized(ConstLedgerStateEntry const& entry);

bool isLiquidating(LedgerEntry const& le);
bool isLiquidating(LedgerStateEntry const& entry);
bool isLiquidating(ConstLedgerStateEntry const& entry);

bool isBaseAsset(AbstractLedgerState& ls, LedgerEntry const& le);
bool isBaseAsset(AbstractLedgerState& ls, LedgerStateEntry const& entry);
bool isBaseAsset(AbstractLedgerState& ls, ConstLedgerStateEntry const& entry);

bool isAuthRequired(ConstLedgerStateEntry const& entry);

bool isImmutableAuth(LedgerStateEntry const& entry);

bool isBaseAssetIssuer(LedgerEntry const& le);
bool isBaseAssetIssuer(LedgerStateEntry const& entry);
bool isBaseAssetIssuer(ConstLedgerStateEntry const& entry);

void normalizeSigners(LedgerStateEntry& entry);

void releaseLiabilities(AbstractLedgerState& ls,
                        LedgerStateHeader const& header,
                        LedgerStateEntry const& offer,
                        bool isMarginTrade = false,
                        int64_t calculatedMaxLiability = 0);

void setAuthorized(LedgerStateEntry& entry, bool authorized);
void setLiquidation(LedgerStateEntry& entry, bool liquidate);
}
