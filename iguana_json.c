/******************************************************************************
 * Copyright © 2014-2015 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "iguana777.h"


struct iguana_jsonitem { struct queueitem DL; uint32_t expired,allocsize; char **retjsonstrp; char jsonstr[]; };

queue_t finishedQ,helperQ;
static struct iguana_info Coins[64];
const char *Hardcoded_coins[][3] = { { "BTC", "bitcoin", "0" }, { "BTCD", "BitcoinDark", "129" } };

struct iguana_info *iguana_coin(const char *symbol)
{
    struct iguana_info *coin; int32_t i = 0;
    if ( symbol == 0 )
    {
        for (i=sizeof(Hardcoded_coins)/sizeof(*Hardcoded_coins); i<sizeof(Coins)/sizeof(*Coins); i++)
        {
            if ( Coins[i].symbol[0] == 0 )
            {
                memset(&Coins[i],0,sizeof(Coins[i]));
                printf("iguana_coin.(new) -> %p\n",&Coins[i]);
                return(&Coins[i]);
            } return(0);
            printf("i.%d (%s) vs name.(%s)\n",i,Coins[i].name,symbol);
        }
    }
    else
    {
        for (i=0; i<sizeof(Hardcoded_coins)/sizeof(*Hardcoded_coins); i++)
        {
            coin = &Coins[i];
            if ( strcmp(symbol,Hardcoded_coins[i][0]) == 0 )
            {
                if ( coin->chain == 0 )
                {
                    strcpy(coin->name,Hardcoded_coins[i][1]);
                    coin->myservices = atoi(Hardcoded_coins[i][2]);
                    strcpy(coin->symbol,symbol);
                    coin->chain = iguana_chainfind(coin->symbol);
                    iguana_initcoin(coin);
                }
                return(coin);
            }
        }
    }
    return(0);
}

char *iguana_genericjson(char *method,cJSON *json)
{
    return(clonestr("{\"result\":\"stub processed generic json\"}"));
}

char *iguana_json(struct iguana_info *coin,char *method,cJSON *json)
{
    return(clonestr("{\"result\":\"stub processed iguana json\"}"));
}

char *iguana_jsonstr(struct iguana_info *coin,char *jsonstr)
{
    cJSON *json; char *retjsonstr,*methodstr;
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( (methodstr= jstr(json,"method")) != 0 )
            retjsonstr = iguana_json(coin,methodstr,json);
        else retjsonstr = clonestr("{\"error\":\"no method in JSON\"}");
        free_json(json);
    } else retjsonstr = clonestr("{\"error\":\"cant parse JSON\"}");
    return(retjsonstr);
}

char *iguana_genericjsonstr(char *jsonstr)
{
    cJSON *json; char *retjsonstr,*methodstr;
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( (methodstr= jstr(json,"method")) != 0 )
            retjsonstr = iguana_genericjson(methodstr,json);
        else retjsonstr = clonestr("{\"error\":\"no method in generic JSON\"}");
        free_json(json);
    } else retjsonstr = clonestr("{\"error\":\"cant parse generic JSON\"}");
    return(retjsonstr);
}

int32_t iguana_processjsonQ(struct iguana_info *coin) // reentrant, can be called during any idletime
{
    struct iguana_jsonitem *ptr;
    if ( (ptr= queue_dequeue(&finishedQ,0)) != 0 )
    {
        if ( ptr->expired != 0 )
        {
            printf("garbage collection: expired.(%s)\n",ptr->jsonstr);
            myfree(ptr,ptr->allocsize);
        } else queue_enqueue("finishedQ",&finishedQ,&ptr->DL,0);
    }
    if ( (ptr= queue_dequeue(&coin->jsonQ,0)) != 0 )
    {
        if ( (*ptr->retjsonstrp= iguana_jsonstr(coin,ptr->jsonstr)) == 0 )
            *ptr->retjsonstrp = clonestr("{\"error\":\"null return from iguana_jsonstr\"}");
        queue_enqueue("finishedQ",&finishedQ,&ptr->DL,0);
        return(1);
    }
    return(0);
}

char *iguana_blockingjsonstr(struct iguana_info *coin,char *jsonstr,uint64_t tag,int32_t maxmillis)
{
    struct iguana_jsonitem *ptr; char *retjsonstr; int32_t len,allocsize; double expiration = milliseconds() + maxmillis;
    if ( coin == 0 )
        return(iguana_genericjsonstr(jsonstr));
    else
    {
        len = (int32_t)strlen(jsonstr);
        allocsize = sizeof(*ptr) + len + 1;
        ptr = mycalloc('J',1,allocsize);
        ptr->allocsize = allocsize;
        ptr->retjsonstrp = &retjsonstr;
        memcpy(ptr->jsonstr,jsonstr,len+1);
        queue_enqueue("jsonQ",&coin->jsonQ,&ptr->DL,0);
        while ( milliseconds() < expiration )
        {
            usleep(100);
            if ( (retjsonstr= *ptr->retjsonstrp) != 0 )
            {
                queue_delete(&finishedQ,&ptr->DL,allocsize,1);
                return(retjsonstr);
            }
            usleep(1000);
        }
        printf("(%s) expired\n",jsonstr);
        ptr->expired = (uint32_t)time(NULL);
        return(clonestr("{\"error\":\"iguana jsonstr expired\"}"));
    }
}

char *iguana_JSON(char *jsonstr)
{
    int32_t iguana_launchcoin(char *symbol,cJSON *json);
    cJSON *json,*retjson; uint64_t tag; uint32_t timeout; int32_t retval;
    struct iguana_info *coin; char *method,*retjsonstr,*symbol,*retstr = 0;
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( (method= jstr(json,"method")) != 0 && strcmp(method,"launch") == 0 )
        {
            if ( (retval= iguana_launchcoin(jstr(json,"coin"),json)) > 0 )
                return(clonestr("{\"result\":\"launched coin\"}"));
            else if ( retval == 0 ) return(clonestr("{\"result\":\"coin already launched\"}"));
            else return(clonestr("{\"error\":\"error launching coin\"}"));
        }
        if ( (tag= j64bits(json,"tag")) == 0 )
            randombytes((uint8_t *)&tag,sizeof(tag));
        if ( (symbol= jstr(json,"coin")) != 0 )
            coin = iguana_coin(symbol);
        else coin = 0;
        if ( (timeout= juint(json,"timeout")) == 0 )
            timeout = IGUANA_JSONTIMEOUT;
        if ( (retjsonstr= iguana_blockingjsonstr(coin,jsonstr,tag,timeout)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retjsonstr)) == 0 )
                retjson = cJSON_Parse("{\"error\":\"cant parse retjsonstr\"}");
            jdelete(retjson,"tag");
            jadd64bits(retjson,"tag",tag);
            retstr = jprint(retjson,1);
            free(retjsonstr);//,strlen(retjsonstr)+1);
        }
        free_json(json);
    } else retstr = clonestr("{\"error\":\"cant parse JSON\"}");
    if ( retstr == 0 )
        retstr = clonestr("{\"error\":\"null return\"}");
    return(retstr);
}

void iguana_issuejsonstrM(void *arg)
{
    cJSON *json; int32_t fd; char *retjsonstr,*jsonstr = arg;
    retjsonstr = iguana_JSON(jsonstr);
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( (fd= juint(json,"retdest")) > 0 )
        {
            send(fd,jsonstr,(int32_t)strlen(jsonstr)+1,MSG_NOSIGNAL);
        }
        free_json(json);
        return;
    }
    printf("%s\n",retjsonstr);
    myfree(retjsonstr,strlen(retjsonstr)+1);
    myfree(jsonstr,strlen(jsonstr)+1);
}

void iguana_helper(void *arg)
{
    int32_t flag; void *ptr; queue_t *Q = arg;
    printf("start helper\n");
    while ( 1 )
    {
        flag = 0;
        if ( (ptr= queue_dequeue(Q,0)) != 0 )
        {
            printf("START emittxdata\n");
            //iguana_emittxdata(bp->coin,bp);
            flag++;
            printf("FINISH emittxdata\n");
        }
    }
    if ( flag == 0 )
        sleep(1);
}

void iguana_main(void *arg)
{
    int32_t i,len,flag; cJSON *json; uint8_t secretbuf[512]; char *coinargs,*secret,*jsonstr = arg;
    //  portable_OS_init()?
    mycalloc(0,0,0);
    iguana_initQ(&helperQ,"helperQ");
    ensure_directory("DB");
    ensure_directory("tmp");
    if ( jsonstr != 0 && (json= cJSON_Parse(jsonstr)) != 0 )
    {
        IGUANA_NUMHELPERS = juint(json,"numhelpers");
        if ( (secret= jstr(json,"secret")) != 0 )
        {
            len = (int32_t)strlen(secret);
            if ( is_hexstr(secret) != 0 && len <= (sizeof(secretbuf)<<1) )
            {
                len >>= 1;
                decode_hex(secretbuf,len,secret);
            } else vcalc_sha256(0,secretbuf,(void *)secret,len), len = sizeof(bits256);
        }
        if ( jobj(json,"coins") != 0 )
            coinargs = jsonstr;
    }
    if ( IGUANA_NUMHELPERS == 0 )
    {
#ifdef __linux__
        IGUANA_NUMHELPERS = 8;
#else
        IGUANA_NUMHELPERS = 1;
#endif
    }
    for (i=0; i<IGUANA_NUMHELPERS; i++)
        iguana_launch("helpers",iguana_helper,&helperQ,IGUANA_HELPERTHREAD);
    if ( coinargs != 0 )
        iguana_launch("iguana_coins",iguana_coins,coinargs,IGUANA_PERMTHREAD);
    else if ( 0 )
    {
        sleep(5);
        iguana_JSON("{\"agent\":\"iguana\",\"method\":\"launch\",\"coin\":\"BTCD\"}");
    }
    while ( 1 )
    {
        flag = 0;
        for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
            if ( Coins[i].symbol[0] != 0 )
                flag += iguana_processjsonQ(&Coins[i]);
        if ( flag == 0 )
            usleep(100000);
    }
}
