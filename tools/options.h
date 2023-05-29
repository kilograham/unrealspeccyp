/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2010 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
Copyright (C) 2023 Graham Sanderson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __OPTION_H__
#define __OPTION_H__

#include "../std.h"
#include "list.h"

#pragma once

namespace xOptions
{

class eOptionB : public eList<eOptionB>
{
public:
	eOptionB() : customizable(true), storeable(true)
#ifdef USE_MU
	, is_action(false), is_change_pending(false)
#endif
	{}
#ifndef NO_USE_DESTRUCTORS
	virtual ~eOptionB() {}
#endif

	bool	Customizable() const { return customizable; }
	bool	Storeable() const { return storeable; }
#ifdef USE_MU
	bool    IsAction() const { return is_action; }
	// some options when Set or Change are called
	bool    IsChangePending() const { return is_change_pending; }
	virtual void Complete(bool accept = true) {
		if (accept) Apply();
	}
#endif

	virtual const char* Name() const = 0;
	virtual const char*	Value() const { return NULL; }
	virtual void Value(const char* v) {}
	virtual const char** Values() const { return NULL; }
	virtual void Change(bool next = true) {}
	virtual void Apply() {}

	static eOptionB* Find(const char* name);
	virtual int Order() const { return 0; }

protected:
	bool 	customizable;
	bool	storeable;
#ifdef USE_MU
	bool    is_action;
	bool    is_change_pending;
#endif
};

template<class T> class eOption : public eOptionB
{
public:
	virtual operator const T&() const { return value; }
	virtual void Set(const T& v) { value = v; }
#ifdef USE_MU
	virtual void SetNow(const T& v) { Set(v); Complete(); }
#endif
	static eOption* Find(const char* name) { return (eOption*)eOptionB::Find(name); }

protected:
	T	value;
};

class eOptionInt : public eOption<int>
{
public:
	eOptionInt() { Set(0); }
	virtual const char*	Value() const;
	virtual void Value(const char* v);
protected:
	void Change(int f, int l, bool next = true);
};

class eOptionIntWithPending : public eOptionInt {
public:
	virtual operator const int&() const override { return is_change_pending ? pending_value : value; }

	void Complete(bool accept) override {
		if (accept && is_change_pending) {
			SetNow(pending_value);
		}
		is_change_pending = false;
	}

	void Set(const int& v) override {
		pending_value = v;
		is_change_pending = (v != value);
	}

	void SetNow(const int & v) override {
		value = v;
		is_change_pending = false;
		Apply();
	}
protected:
	int pending_value;
};

class eOptionBool : public eOption<bool>
{
public:
	eOptionBool() { Set(false); }
	virtual const char*	Value() const;
	virtual void Value(const char* v);
	virtual const char** Values() const;
	virtual void Change(bool next = true) { Set(!(bool)*this); }
};

class eOptionBoolWithPending : public eOptionBool {
public:
	virtual operator const bool&() const override { return is_change_pending ? pending_value : value; }

	void Complete(bool accept) override {
		if (accept && is_change_pending) {
			SetNow(pending_value);
		}
		is_change_pending = false;
	}

	void Set(const bool& v) override {
		pending_value = v;
		is_change_pending = (v != value);
	}

	void SetNow(const bool & v) override {
		value = v;
		is_change_pending = false;
		Apply();
	}
protected:
	bool pending_value;
};

class eOptionBoolYesNo : public eOptionBool
{
public:
	virtual const char** Values() const;
};

class eOptionString : public eOption<const char*>
{
public:
	eOptionString() : alloc_size(32) { value = new char[alloc_size]; Value(""); }
#ifndef NO_USE_DESTRUCTORS
	virtual ~eOptionString() { SAFE_DELETE_ARRAY(value); }
#endif
	virtual const char*	Value() const { return value; }
	virtual void Value(const char* v);
	virtual void Set(const char*& v) { Value(v); }
private:
	int alloc_size;
};

void Load();
void Store();

}
//namespace xOptions

#endif//__OPTION_H__
