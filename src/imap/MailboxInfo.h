#ifndef IMAP_MAILBOX_INFO_H
#define IMAP_MAILBOX_INFO_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"

namespace ReadyMailIMAP
{
    typedef struct mailbox_info
    {
        uint32_t msgCount = 0, RecentCount = 0, UIDValidity = 0, nextUID = 0, UnseenIndex = 0, highestModseq;
        bool noModseq = false;
        std::vector<String> flags, permanentFlags;
        String name;
    } MailboxInfo;
}

#endif
#endif
