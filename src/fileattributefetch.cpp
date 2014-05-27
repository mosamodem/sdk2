/**
 * @file fileattributefetch.cpp
 * @brief Classes for file attributes fetching
 *
 * (c) 2013-2014 by Mega Limited, Wellsford, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include "mega/fileattributefetch.h"
#include "mega/megaclient.h"
#include "mega/megaapp.h"

namespace mega {
FileAttributeFetchChannel::FileAttributeFetchChannel()
{
    req.binary = true;
}

FileAttributeFetch::FileAttributeFetch(handle h, fatype t, int c, int ctag)
{
    nodehandle = h;
    type = t;
    fac = c;
    retries = 0;
    tag = ctag;
}

// post pending requests for this cluster to supplied URL
void FileAttributeFetchChannel::dispatch(MegaClient* client, int fac, const char* targeturl)
{
    req.out->clear();

    // dispatch all pending fetches for this channel's cluster
    for (faf_map::iterator it = client->fafs.begin(); it != client->fafs.end(); it++)
    {
        if (it->second->fac == fac)
        {
            // prevent reallocations
            req.out->reserve(client->fafs.size() * sizeof(handle));

            it->second->dispatched = 1;

            req.out->append((char*)&it->first, sizeof(handle));
        }
    }

    completed = 0;
    
    req.posturl = targeturl;
    req.post(client);
}

// communicate the result of a file attribute fetch to the application and
// remove completed records
void FileAttributeFetchChannel::parse(MegaClient* client, int fac, bool final)
{
#pragma pack(push,1)
    struct FaPos
    {
        handle h;
        uint32_t pos;
    };
#pragma pack(pop)

    // data is structured as (handle.8.le / position.4.le)* attribute data
    // attributes are CBC-encrypted with the file's key

    // we must have received at least one full header to continue
    if (req.in.size() < sizeof(FaPos))
    {
        if (final) client->faf_failed(fac);
        return;
    }

    uint32_t bod = ((FaPos*)req.in.data())->pos;

    if (req.in.size() < bod)
    {
        if (final) client->faf_failed(fac);
        return;
    }

    const char* fadata;
    uint32_t falen, fapos;
    Node* n;
    faf_map::iterator it;

    fadata = req.in.data();

    while (completed < bod - (final ? 0 : sizeof(FaPos))
        && req.in.size() >= (falen = (final ? req.in.size() : ((FaPos*)(req.in.data() + completed))[1].pos)))
    {
        it = client->fafs.find(((FaPos*)(req.in.data() + completed))->h);

        // locate fetch request (could have been deleted by the application in the meantime)
        if (it != client->fafs.end())
        {
            // locate related node (could have been deleted)
            if ((n = client->nodebyhandle(it->second->nodehandle)))
            {
                fapos = ((FaPos*)(req.in.data() + completed))->pos;
                falen -= fapos;

                if (!(falen & (SymmCipher::BLOCKSIZE - 1)))
                {
                    n->key.cbc_decrypt((byte*)fadata + fapos, falen);

                    client->restag = it->second->tag;

                    client->app->fa_complete(n, it->second->type, fadata + fapos, falen);

                    delete it->second;
                    client->fafs.erase(it);
                }
                else
                {
                    return client->faf_failed(fac);
                }
            }
        }
        else
        {
            return client->faf_failed(fac);
        }
        
        completed += sizeof(FaPos);
    }
    
    if (final && completed != bod)
    {
        client->faf_failed(fac);
    }
}
} // namespace
