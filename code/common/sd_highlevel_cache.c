/*******************************************************************************
* sd_highlevel_cache.c
* version: 1.0
* date: April 16, 2013
* author: Kent deVillafranca (kent@kentdev.net)
* description: Code to handle block caching operations, called by functions in
*              sd_highlevel.c.  The cache is represented as a linked list of
*              nodes; nodes are moved to the head of the list each time they are
*              accessed, and the node at the end of the list is flushed if room
*              is needed in the cache for a new block.
*******************************************************************************/

#include "sd_highlevel.h"
#include "debug.h"

cached_sector cache[CACHED_SECTORS];
cached_sector *head;

// initialize the cache chain, each node containing block 0xffffffff
void init_cache (void)
{
    for (uint8_t i = 0; i < CACHED_SECTORS; i++)
    {
        cache[i].block_number = INVALID_SECTOR;
        cache[i].modified = false;
        
        if (i == 0)
            head = &cache[i];
        
        if (i == CACHED_SECTORS - 1)
            cache[i].next = END_OF_CHAIN;
        else
            cache[i].next = &cache[i + 1];
    }
}

// find a node in the cache chain
cached_sector *cache_lookup (uint32_t block_number)
{
    cached_sector *c = head;
    
    #ifdef HIGHLEVEL_CACHE_DEBUG
    debug ("Searching cache for block ");
    debugulong (block_number);
    debug ("\n");
    #endif
    
    while (c != END_OF_CHAIN)
    {
        #ifdef HIGHLEVEL_CACHE_DEBUG
        debug ("\t");
        debughex ((uint16_t)c);
        debug (" (");
        debugulong (c->block_number);
        debug (") -> ");
        debughex ((uint16_t)c->next);
        debug ("\n");
        #endif
        
        if (c->block_number == block_number)
            return c;
        c = c->next;
    }
    
    return END_OF_CHAIN;
}

// remove a node from the chain and make it the new head
bool move_to_head (cached_sector *node)
{
    cached_sector *c = head;
    
    if (head == node)
        return true;
    
    while (c->next != node)
    {  // loop until we find the node pointing to the target node
        c = c->next;
        
        if (c->next == END_OF_CHAIN)
        {
            error_code = ERROR_CACHE_FAILURE;
            return false;
        }
    }
    
    // remove the target node from the chain
    c->next = node->next;
    
    // make the target node point to the current head
    node->next = head;
    
    // make the target node the new head
    head = node;
    
    error_code = ERROR_NONE;
    return true;
}

// remove and return the last node of the chain
cached_sector *remove_least_used (void)
{
    cached_sector *c = head;
    cached_sector *next = c->next;
    
    if (head->next == END_OF_CHAIN)
    {  // if the head node's next points to END_OF_CHAIN, then head is the ONLY node in the chain
        head = END_OF_CHAIN;  // set head to END_OF_CHAIN
        return c;
    }
    
    while (next->next != END_OF_CHAIN)
    {  // loop until we find the node pointing to the final node
        c = next;
        next = c->next;
    }
    
    // make the node pointing to the final node point to END_OF_CHAIN instead
    c->next = END_OF_CHAIN;
    return next;
}

// add a new node as the head of the chain
void add_as_head (cached_sector *node)
{
    node->next = head;
    head = node;
}

// write out any modified cached sectors, then re-initialize the cache chain
bool flush_cache (void)
{
    bool result = true;
    
    for (uint8_t i = 0; i < CACHED_SECTORS; i++)
    {
        // if we hit an error, keep going and return the error later
        result = result && write_to_card (&cache[i]);
    }
    
    init_cache();
    
    return result;
}

