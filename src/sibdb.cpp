// Copyright (c) 2015 The Sibcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sibdb.h"

using namespace boost;
using namespace std;


bool CSibDB::WriteName(const std::string& key, const std::string& arr)
{
    return Write(key, arr);
}

bool CSibDB::ReadName(const std::string& key, std::string& arr)
{
    return Read(key, arr);
}


bool CSibDB::EraseName(const std::string& key)
{
    return Erase(key);
}

