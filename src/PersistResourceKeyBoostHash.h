///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
// optimizes the DBPF loading.
//
// Copyright (c) 2024, 2025 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "cGZPersistResourceKey.h"
#include "boost/functional/hash.hpp"

// a hash implementation of cGZPersistResourceKey for use in boost collections
template<> struct boost::hash<const cGZPersistResourceKey>
{
	size_t operator()(const cGZPersistResourceKey& key) const noexcept
	{
		size_t seed = 0;

		boost::hash_combine(seed, key.type);
		boost::hash_combine(seed, key.instance);
		boost::hash_combine(seed, key.group);

		return seed;
	}
};
