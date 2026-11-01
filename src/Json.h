/*
 * Copyright 2026, Andrea Bernardi
 * SPDX-License-Identifier: MIT
 */
#ifndef JSON_H
#define JSON_H


#include <cstddef>
#include <map>
#include <string>
#include <vector>


/*!	A small JSON value, for the lines that Cinema and cinema-helper exchange.
	It keeps only what the protocol needs, and no dependencies: numbers are
	doubles, and ToString() writes whole ones without a decimal part.
*/
class Json {
public:
	enum Type {
		kNull,
		kBool,
		kNumber,
		kString,
		kArray,
		kObject
	};

								Json();
								Json(bool value);
								Json(int value);
								Json(double value);
								Json(const char* value);
								Json(const std::string& value);

	static	Json				Parse(const std::string& text,
									bool* ok = NULL);
	static	Json				MakeObject();
	static	Json				MakeArray();

			std::string			ToString() const;

			Type				GetType() const { return fType; }
			bool				IsNull() const { return fType == kNull; }
			bool				IsArray() const { return fType == kArray; }
			bool				IsObject() const { return fType == kObject; }

			bool				Bool(bool fallback = false) const;
			int					Int(int fallback = 0) const;
			double				Number(double fallback = 0) const;
			const std::string&	String() const;

			const Json*			Find(const char* key) const;
			std::string			GetString(const char* key,
									const char* fallback = "") const;
			int					GetInt(const char* key,
									int fallback = 0) const;
			double				GetDouble(const char* key,
									double fallback = 0) const;
			bool				GetBool(const char* key,
									bool fallback = false) const;
			std::vector<Json>&	Elements() { return fArray; }
			const std::vector<Json>& Elements() const { return fArray; }

			void				Set(const char* key, const Json& value);
			void				Add(const Json& value);

private:
			void				_Write(std::string& out) const;

			Type				fType;
			bool				fBool;
			double				fNumber;
			std::string			fString;
			std::vector<Json>	fArray;
			std::map<std::string, Json>	fObject;
};


#endif	// JSON_H
