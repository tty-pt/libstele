#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <ttypt/qsys.h>

static napi_value QsysNowMs(napi_env env, napi_callback_info info) {
    (void)info;
    uint64_t now = qsys_now_ms();
    napi_value res;
    napi_create_bigint_uint64(env, now, &res);
    return res;
}

static napi_value QsysSelfRssBytes(napi_env env, napi_callback_info info) {
    (void)info;
    size_t rss = qsys_self_rss_bytes();
    napi_value res;
    napi_create_int64(env, (int64_t)rss, &res);
    return res;
}

static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"nowMs", NULL, QsysNowMs, NULL, NULL, NULL, napi_default, NULL},
        {"selfRssBytes", NULL, QsysSelfRssBytes, NULL, NULL, NULL, napi_default, NULL}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
