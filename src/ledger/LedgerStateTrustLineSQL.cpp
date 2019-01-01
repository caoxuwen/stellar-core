// Copyright 2017 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "crypto/KeyUtils.h"
#include "crypto/SecretKey.h"
#include "database/Database.h"
#include "ledger/LedgerStateImpl.h"
#include "transactions/TransactionUtils.h"
#include "util/XDROperators.h"
#include "util/types.h"

namespace stellar
{

std::shared_ptr<LedgerEntry const>
LedgerStateRoot::Impl::loadTrustLine(LedgerKey const& key) const
{
    auto const& asset = key.trustLine().asset;
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        throw std::runtime_error("IONX TrustLine?");
    }
    else if (key.trustLine().accountID == getIssuer(asset))
    {
        throw std::runtime_error("TrustLine accountID is issuer");
    }

    if (isDebtAsset(key.trustLine().asset))
    {
        return loadDebtTrustLine(key);
    }

    std::string actIDStrKey = KeyUtils::toStrKey(key.trustLine().accountID);
    std::string issuerStr, assetStr;
    if (asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        assetCodeToStr(asset.alphaNum4().assetCode, assetStr);
        issuerStr = KeyUtils::toStrKey(asset.alphaNum4().issuer);
    }
    else if (asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        assetCodeToStr(asset.alphaNum12().assetCode, assetStr);
        issuerStr = KeyUtils::toStrKey(asset.alphaNum12().issuer);
    }

    Liabilities liabilities;
    soci::indicator buyingLiabilitiesInd, sellingLiabilitiesInd;

    LedgerEntry le;
    le.data.type(TRUSTLINE);
    TrustLineEntry& tl = le.data.trustLine();

    auto prep = mDatabase.getPreparedStatement(
        "SELECT tlimit, balance, flags, debt, lastmodified, buyingliabilities, "
        "sellingliabilities FROM trustlines "
        "WHERE accountid= :id AND issuer= :issuer AND assetcode= :asset");
    auto& st = prep.statement();
    st.exchange(soci::into(tl.limit));
    st.exchange(soci::into(tl.balance));
    st.exchange(soci::into(tl.flags));
    st.exchange(soci::into(tl.debt));
    st.exchange(soci::into(le.lastModifiedLedgerSeq));
    st.exchange(soci::into(liabilities.buying, buyingLiabilitiesInd));
    st.exchange(soci::into(liabilities.selling, sellingLiabilitiesInd));
    st.exchange(soci::use(actIDStrKey));
    st.exchange(soci::use(issuerStr));
    st.exchange(soci::use(assetStr));
    st.define_and_bind();
    {
        auto timer = mDatabase.getSelectTimer("trust");
        st.execute(true);
    }
    if (!st.got_data())
    {
        return nullptr;
    }

    tl.accountID = key.trustLine().accountID;
    tl.asset = key.trustLine().asset;

    assert(buyingLiabilitiesInd == sellingLiabilitiesInd);
    if (buyingLiabilitiesInd == soci::i_ok)
    {
        tl.ext.v(1);
        tl.ext.v1().liabilities = liabilities;
    }

    return std::make_shared<LedgerEntry>(std::move(le));
}

std::shared_ptr<LedgerEntry const>
LedgerStateRoot::Impl::loadDebtTrustLine(LedgerKey const& key) const
{
    std::string actIDStrKey = KeyUtils::toStrKey(key.trustLine().accountID);

    Liabilities liabilities;
    soci::indicator buyingLiabilitiesInd, sellingLiabilitiesInd;

    LedgerEntry le;
    le.data.type(TRUSTLINE);
    TrustLineEntry& tl = le.data.trustLine();

    auto prep = mDatabase.getPreparedStatement(
        "SELECT tlimit, balance, flags, debt, lastmodified, buyingliabilities, "
        "sellingliabilities FROM trustlines "
        "WHERE accountid= :id AND debt > 0");
    auto& st = prep.statement();
    st.exchange(soci::into(tl.limit));
    st.exchange(soci::into(tl.balance));
    st.exchange(soci::into(tl.flags));
    st.exchange(soci::into(tl.debt));
    st.exchange(soci::into(le.lastModifiedLedgerSeq));
    st.exchange(soci::into(liabilities.buying, buyingLiabilitiesInd));
    st.exchange(soci::into(liabilities.selling, sellingLiabilitiesInd));
    st.exchange(soci::use(actIDStrKey));

    st.define_and_bind();
    {
        auto timer = mDatabase.getSelectTimer("trust");
        st.execute(true);
    }
    if (!st.got_data())
    {
        return nullptr;
    }

    tl.accountID = key.trustLine().accountID;
    tl.asset = key.trustLine().asset;

    assert(buyingLiabilitiesInd == sellingLiabilitiesInd);
    if (buyingLiabilitiesInd == soci::i_ok)
    {
        tl.ext.v(1);
        tl.ext.v1().liabilities = liabilities;
    }

    return std::make_shared<LedgerEntry>(std::move(le));
}

std::vector<LedgerEntry>
LedgerStateRoot::Impl::loadDebtHolders(Asset const& asset) const
{
    if (asset.type() == ASSET_TYPE_NATIVE)
    {
        throw std::runtime_error("debt holder should not be native asset");
    }

    std::string issuerStr, assetStr;
    if (asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        assetCodeToStr(asset.alphaNum4().assetCode, assetStr);
        issuerStr = KeyUtils::toStrKey(asset.alphaNum4().issuer);
    }
    else if (asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        assetCodeToStr(asset.alphaNum12().assetCode, assetStr);
        issuerStr = KeyUtils::toStrKey(asset.alphaNum12().issuer);
    }

    std::vector<LedgerEntry> trustlines;
    std::string accountid_str;

    LedgerEntry le;
    le.data.type(TRUSTLINE);
    TrustLineEntry& tl = le.data.trustLine();
    Liabilities liabilities;
    soci::indicator buyingLiabilitiesInd, sellingLiabilitiesInd;

    auto prep = mDatabase.getPreparedStatement(
        "SELECT accountid, tlimit, balance, flags, debt, lastmodified, "
        "buyingliabilities, "
        "sellingliabilities FROM trustlines "
        "WHERE issuer= :issuer AND assetcode= :asset AND debt != 0");
    auto& st = prep.statement();
    st.exchange(soci::into(accountid_str));
    st.exchange(soci::into(tl.limit));
    st.exchange(soci::into(tl.balance));
    st.exchange(soci::into(tl.flags));
    st.exchange(soci::into(tl.debt));
    st.exchange(soci::into(le.lastModifiedLedgerSeq));
    st.exchange(soci::into(liabilities.buying, buyingLiabilitiesInd));
    st.exchange(soci::into(liabilities.selling, sellingLiabilitiesInd));
    st.exchange(soci::use(issuerStr));
    st.exchange(soci::use(assetStr));
    st.define_and_bind();
    {
        auto timer = mDatabase.getSelectTimer("trust");
        st.execute(true);
    }

    while (st.got_data())
    {
        tl.asset = asset;

        assert(buyingLiabilitiesInd == sellingLiabilitiesInd);
        if (buyingLiabilitiesInd == soci::i_ok)
        {
            tl.ext.v(1);
            tl.ext.v1().liabilities = liabilities;
        }
        tl.accountID = KeyUtils::fromStrKey<PublicKey>(accountid_str);

        trustlines.emplace_back(le);
        st.fetch();
    }

    return trustlines;
}

std::vector<LedgerEntry>
LedgerStateRoot::Impl::loadLiquidationCandidates(
    Asset const& asset1, double ratio1, Asset const& asset2, double ratio2,
    Asset const& assetBalance) const
{

    if (asset1.type() == ASSET_TYPE_NATIVE ||
        asset2.type() == ASSET_TYPE_NATIVE)
    {
        throw std::runtime_error("debt holder should not be native asset");
    }

    std::string issuerStr1, assetStr1;
    if (asset1.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        assetCodeToStr(asset1.alphaNum4().assetCode, assetStr1);
        issuerStr1 = KeyUtils::toStrKey(asset1.alphaNum4().issuer);
    }
    else if (asset1.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        assetCodeToStr(asset1.alphaNum12().assetCode, assetStr1);
        issuerStr1 = KeyUtils::toStrKey(asset1.alphaNum12().issuer);
    }

    std::string issuerStr2, assetStr2;
    if (asset2.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        assetCodeToStr(asset2.alphaNum4().assetCode, assetStr2);
        issuerStr2 = KeyUtils::toStrKey(asset2.alphaNum4().issuer);
    }
    else if (asset2.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        assetCodeToStr(asset2.alphaNum12().assetCode, assetStr2);
        issuerStr2 = KeyUtils::toStrKey(asset2.alphaNum12().issuer);
    }

    std::vector<LedgerEntry> trustlines;
    std::string accountid_str;

    LedgerEntry le;
    le.data.type(TRUSTLINE);
    TrustLineEntry& tl = le.data.trustLine();

    auto prep = mDatabase.getPreparedStatement(
        "SELECT coin1.accountid, coin1.balance, coin1.flags, coin1.debt, "
        "coin2.debt "
        "FROM trustlines as coin1 LEFT JOIN trustlines as coin2 on "
        "coin1.accountid=coin2.accountid "
        "WHERE coin1.issuer = :issuer1 AND coin1.assetcode = :asset1 "
        "coin2.issuer = :issuer2 AND coin2.assetcode = :asset2");
    auto& st = prep.statement();
    st.exchange(soci::into(accountid_str));
    st.exchange(soci::into(tl.limit));
    st.exchange(soci::into(tl.balance));
    st.exchange(soci::into(tl.flags));
    st.exchange(soci::into(tl.debt));
    st.exchange(soci::use(issuerStr1));
    st.exchange(soci::use(assetStr1));
    st.exchange(soci::use(issuerStr2));
    st.exchange(soci::use(assetStr2));
    st.define_and_bind();
    {
        auto timer = mDatabase.getSelectTimer("trust");
        st.execute(true);
    }

    while (st.got_data())
    {
        tl.asset = asset1;

        tl.accountID = KeyUtils::fromStrKey<PublicKey>(accountid_str);

        trustlines.emplace_back(le);
        st.fetch();
    }

    return trustlines;
}

void
LedgerStateRoot::Impl::insertOrUpdateTrustLine(LedgerEntry const& entry,
                                               bool isInsert)
{
    auto const& tl = entry.data.trustLine();

    std::string actIDStrKey = KeyUtils::toStrKey(tl.accountID);
    unsigned int assetType = tl.asset.type();
    std::string issuerStr, assetCode;
    if (tl.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        issuerStr = KeyUtils::toStrKey(tl.asset.alphaNum4().issuer);
        assetCodeToStr(tl.asset.alphaNum4().assetCode, assetCode);
    }
    else if (tl.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        issuerStr = KeyUtils::toStrKey(tl.asset.alphaNum12().issuer);
        assetCodeToStr(tl.asset.alphaNum12().assetCode, assetCode);
    }
    if (actIDStrKey == issuerStr)
    {
        throw std::runtime_error("Issuer's own trustline should not be used "
                                 "outside of OperationFrame");
    }

    Liabilities liabilities;
    soci::indicator liabilitiesInd = soci::i_null;
    if (tl.ext.v() == 1)
    {
        liabilities = tl.ext.v1().liabilities;
        liabilitiesInd = soci::i_ok;
    }

    std::string sql;
    if (isInsert)
    {
        sql =
            "INSERT INTO trustlines "
            "(accountid, assettype, issuer, assetcode, balance, debt, tlimit, "
            "flags, lastmodified, buyingliabilities, sellingliabilities) "
            "VALUES (:id, :at, :iss, :ac, :b, :dt, :tl, :f, :lm, :bl, :sl)";
    }
    else
    {
        sql =
            "UPDATE trustlines "
            "SET balance=:b, tlimit=:tl, debt=:dt, flags=:f, lastmodified=:lm, "
            "buyingliabilities=:bl, sellingliabilities=:sl "
            "WHERE accountid=:id AND issuer=:iss AND assetcode=:ac";
    }
    auto prep = mDatabase.getPreparedStatement(sql);
    auto& st = prep.statement();
    st.exchange(soci::use(actIDStrKey, "id"));
    if (isInsert)
    {
        st.exchange(soci::use(assetType, "at"));
    }
    st.exchange(soci::use(issuerStr, "iss"));
    st.exchange(soci::use(assetCode, "ac"));
    st.exchange(soci::use(tl.balance, "b"));
    st.exchange(soci::use(tl.debt, "dt"));
    st.exchange(soci::use(tl.limit, "tl"));
    st.exchange(soci::use(tl.flags, "f"));
    st.exchange(soci::use(entry.lastModifiedLedgerSeq, "lm"));
    st.exchange(soci::use(liabilities.buying, liabilitiesInd, "bl"));
    st.exchange(soci::use(liabilities.selling, liabilitiesInd, "sl"));
    st.define_and_bind();
    {
        auto timer = isInsert ? mDatabase.getInsertTimer("trust")
                              : mDatabase.getUpdateTimer("trust");
        st.execute(true);
    }
    if (st.get_affected_rows() != 1)
    {
        throw std::runtime_error("Could not update data in SQL");
    }
}

void
LedgerStateRoot::Impl::deleteTrustLine(LedgerKey const& key)
{
    auto const& tl = key.trustLine();

    std::string actIDStrKey = KeyUtils::toStrKey(tl.accountID);
    std::string issuerStr, assetCode;
    if (tl.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
    {
        issuerStr = KeyUtils::toStrKey(tl.asset.alphaNum4().issuer);
        assetCodeToStr(tl.asset.alphaNum4().assetCode, assetCode);
    }
    else if (tl.asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
    {
        issuerStr = KeyUtils::toStrKey(tl.asset.alphaNum12().issuer);
        assetCodeToStr(tl.asset.alphaNum12().assetCode, assetCode);
    }
    if (actIDStrKey == issuerStr)
    {
        throw std::runtime_error("Issuer's own trustline should not be used "
                                 "outside of OperationFrame");
    }

    auto prep = mDatabase.getPreparedStatement(
        "DELETE FROM trustlines "
        "WHERE accountid=:v1 AND issuer=:v2 AND assetcode=:v3");
    auto& st = prep.statement();
    st.exchange(soci::use(actIDStrKey));
    st.exchange(soci::use(issuerStr));
    st.exchange(soci::use(assetCode));
    st.define_and_bind();
    {
        auto timer = mDatabase.getDeleteTimer("trust");
        st.execute(true);
    }
    if (st.get_affected_rows() != 1)
    {
        throw std::runtime_error("Could not update data in SQL");
    }
}

void
LedgerStateRoot::Impl::dropTrustLines()
{
    throwIfChild();
    mEntryCache.clear();
    mBestOffersCache.clear();

    mDatabase.getSession() << "DROP TABLE IF EXISTS trustlines;";
    mDatabase.getSession()
        << "CREATE TABLE trustlines"
           "("
           "accountid    VARCHAR(56)     NOT NULL,"
           "assettype    INT             NOT NULL,"
           "issuer       VARCHAR(56)     NOT NULL,"
           "assetcode    VARCHAR(12)     NOT NULL,"
           "tlimit       BIGINT          NOT NULL CHECK (tlimit > 0),"
           "balance      BIGINT          NOT NULL CHECK (balance >= 0),"
           "flags        INT             NOT NULL,"
           "lastmodified INT             NOT NULL,"
           "PRIMARY KEY  (accountid, issuer, assetcode)"
           ");";
}
}
