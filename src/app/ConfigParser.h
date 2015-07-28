/** 
 * Copyright (c) 2015, Affinegy, Inc.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any 
 * purpose with or without fee is hereby granted, provided that the above 
 * copyright notice and this permission notice appear in all copies. 
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES 
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY 
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES 
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION 
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */ 

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>

class ConfigParser
{
    public:
        ConfigParser( const std::string& filepath );
        ~ConfigParser();

        std::string GetField(const char* field);
        std::map<std::string, std::string> GetConfigMap();
        std::vector<std::string> GetRoster() const;
        std::vector<std::string> GetErrors() const;
        int SetField(const char* field, const char* value);
        int GetPort();
        int SetPort(int value);
        int SetRoster(std::vector<std::string> roster);
        bool isConfigValid();
    private:
        std::map<std::string, std::string> options;
        mutable std::vector<std::string> errors;
        const std::string configPath;

        ConfigParser() {} // Private to prevent use
};
#endif
