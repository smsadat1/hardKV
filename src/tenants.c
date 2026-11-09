#include "tinycache.h"

// tenant isolation enforcements

// tenant_id only allows alphanumeric 
bool verify_tenant_id(char* tenant_id) {

    for(size_t i = 0; tenant_id[i] != '\0'; i++) {
        if(!isalnum(tenant_id[i]))
            return false;
    }
    return true;
}
