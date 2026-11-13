#include <node_api.h>
#include <stdint.h>

#include <ttypt/stele.h>

static napi_value SteleNowMs(napi_env env, napi_callback_info info) {
    (void)info;
    uint64_t now = stl_now_ms();
    napi_value res;
    napi_create_bigint_uint64(env, now, &res);
    return res;
}

static napi_value SteleSelfRssBytes(napi_env env, napi_callback_info info) {
    (void)info;
    uint64_t rss = stl_self_rss_bytes();
    napi_value res;
    napi_create_int64(env, (int64_t)rss, &res);
    return res;
}

static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"nowMs", NULL, SteleNowMs, NULL, NULL, NULL, napi_default, NULL},
        {"selfRssBytes", NULL, SteleSelfRssBytes, NULL, NULL, NULL, napi_default, NULL}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
