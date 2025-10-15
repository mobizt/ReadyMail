/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef READY_UTILS_BASE__H
#define READY_UTILS_BASE__H
#include <Arduino.h>
#include <type_traits>

#if defined(ENABLE_IMAP) || defined(ENABLE_SMTP)
class IPChecker
{

public:
    IPChecker() {}

    bool isValidHost(const char *s)
    {
        bool invalid = isValidIP(s) == 0 || (isValidIP(s) == -1 && !isValidDomain(s));
        return !invalid;
    }

    int isValidIP(const char *ip)
    {
        int len = strlen(ip);
        if (len == 0)
            return -1;

        char segment[4];
        int segIndex = 0, segCount = 0;

        for (int i = 0; i <= len; i++)
        {
            if (ip[i] == '.' || ip[i] == '\0')
            {
                if (segIndex == 0)
                    return -1;

                segment[segIndex] = '\0';
                int ret = valid_part(segment);
                if (ret <= 0)
                    return ret;

                segIndex = 0;
                segCount++;
            }
            else if (isdigit(ip[i]))
            {
                if (segIndex >= 3)
                    return -1;

                segment[segIndex++] = ip[i];
            }
            else
                return -1;
        }

        return segCount == 4 ? 1 : 0;
    }

    int isValidDomain(const char *domain)
    {
        int len = strlen(domain);

        if (len < 3 || len > 253)
            return false;

        if (!isalnum(domain[0]) || !isalnum(domain[len - 1]))
            return false;

        int dot_count = 0;
        for (int i = 0; i < len; i++)
        {
            if (isalnum(domain[i]) || domain[i] == '-')
                continue;
            else if (domain[i] == '.')
            {
                dot_count++;
                if (i == 0 || domain[i - 1] == '.' || i == len - 1)
                    return false;
            }
            else
                return false;
        }
        return dot_count > 0;
    }

private:
    int valid_part(const char *s)
    {
        int len = strlen(s);
        if (len == 0 || len > 3 || (s[0] == '0' && len > 1))
            return -1;

        for (int i = 0; i < len; i++)
        {
            if (!isdigit(s[i]))
                return -1;
        }

        int num = 0;
        for (int i = 0; i < len; i++)
            num = num * 10 + (s[i] - '0');

        if (num == 0 || num > 255)
            return 0;

        return 1;
    }
};

#endif
#endif