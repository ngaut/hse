/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2020 Micron Technology, Inc.  All rights reserved.
 */

#include <hse_ut/framework.h>

#include <hse_ikvdb/mclass_policy.h>
#include <hse_ikvdb/hse_params_internal.h>
#include <mpool/mpool.h>

/*
 * Pre and Post Functions
 */
static int
general_pre(struct mtf_test_info *ti)
{
    return 0;
}

MTF_BEGIN_UTEST_COLLECTION(mclass_policy_test)

MTF_DEFINE_UTEST_PRE(mclass_policy_test, mclass_policy_default, general_pre)
{
    int                  i, j, k, l;
    merr_t               err;
    struct mclass_policy policies[HSE_MPOLICY_COUNT], pol_small[2];
    struct mclass_policy dpolicies[4];
    struct hse_params *  params;
    const char *         test_policy = "api_version: 1\n"
                              "mclass_policies:\n"
                              "  test_only:\n"
                              "    internal:\n"
                              "      values: [ capacity ]\n";

    /* Capacity only media class policy, use capacity for all combinations */
    strcpy(dpolicies[0].mc_name, "capacity_only");
    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++) {
            dpolicies[0].mc_table[i][j][0] = HSE_MPOLICY_MEDIA_CAPACITY;
            dpolicies[0].mc_table[i][j][1] = HSE_MPOLICY_MEDIA_INVALID;
        }

    /* Staging only media class policy, use staging for all combinations  */
    strcpy(dpolicies[1].mc_name, "staging_only");
    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++) {
            dpolicies[1].mc_table[i][j][0] = HSE_MPOLICY_MEDIA_STAGING;
            dpolicies[1].mc_table[i][j][1] = HSE_MPOLICY_MEDIA_INVALID;
        }

    /*
     * staging_capacity_nofallback - only leaf values use capacity.
     * use staging with no fallback to capacity for other combinations.
     */
    strcpy(dpolicies[2].mc_name, "staging_capacity_nofallback");
    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++) {
            dpolicies[2].mc_table[i][j][0] = HSE_MPOLICY_MEDIA_STAGING;
            dpolicies[2].mc_table[i][j][1] = HSE_MPOLICY_MEDIA_INVALID;
        }
    dpolicies[2].mc_table[HSE_MPOLICY_AGE_LEAF][HSE_MPOLICY_DTYPE_VALUE][0] =
        HSE_MPOLICY_MEDIA_CAPACITY;

    /*
     * staging_capacity_fallback - only leaf values use capacity.
     * use staging with fallback to capacity for other combinations.
     */
    strcpy(dpolicies[3].mc_name, "staging_capacity_fallback");
    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++) {
            dpolicies[3].mc_table[i][j][0] = HSE_MPOLICY_MEDIA_STAGING;
            dpolicies[3].mc_table[i][j][1] = HSE_MPOLICY_MEDIA_CAPACITY;
        }
    dpolicies[3].mc_table[HSE_MPOLICY_AGE_LEAF][HSE_MPOLICY_DTYPE_VALUE][0] =
        HSE_MPOLICY_MEDIA_CAPACITY;
    dpolicies[3].mc_table[HSE_MPOLICY_AGE_LEAF][HSE_MPOLICY_DTYPE_VALUE][1] =
        HSE_MPOLICY_MEDIA_INVALID;

    hse_params_to_mclass_policies(NULL, NULL, 0);
    hse_params_to_mclass_policies(NULL, policies, sizeof(policies) / sizeof(policies[0]));

    /* Validate that the parsed policies match the hardcoded matrices for the default policies. */
    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++)
            for (k = 0; k < HSE_MPOLICY_MEDIA_CNT; k++)
                for (l = 0; l < 4; l++) {
                    enum hse_mclass_policy_media hse_mtype;
                    enum mp_media_classp         mpool_mtype;

                    hse_mtype = policies[l].mc_table[i][j][k];

                    ASSERT_EQ(hse_mtype, dpolicies[l].mc_table[i][j][k]);
                    ASSERT_EQ(strcmp(policies[l].mc_name, dpolicies[l].mc_name), 0);

                    mpool_mtype = mclass_policy_get_type(&policies[l], i, j, k);
                    if (hse_mtype == HSE_MPOLICY_MEDIA_INVALID)
                        ASSERT_EQ(mpool_mtype, MP_MED_INVALID);
                    else if (hse_mtype == HSE_MPOLICY_MEDIA_STAGING)
                        ASSERT_EQ(mpool_mtype, MP_MED_STAGING);
                    else
                        ASSERT_EQ(mpool_mtype, MP_MED_CAPACITY);
                }

    /* Test that this doesn't cause a crash because the number of entries is too small
     * to accommodate all four default policies */
    hse_params_to_mclass_policies(NULL, pol_small, sizeof(pol_small) / sizeof(pol_small[0]));

    /*
     * Initialize hse params from a test policy that specifies only <internal, leaf>
     * and validate that the remaining entries are populated from the default template
     * i.e. staging_capacity_nofallback
     */
    err = hse_params_create(&params);
    ASSERT_EQ(err, 0);
    hse_params_from_string(params, test_policy);
    hse_params_to_mclass_policies(params, policies, sizeof(policies) / sizeof(policies[0]));
    ASSERT_EQ(strcmp(policies[4].mc_name, "test_only"), 0);

    for (i = 0; i < HSE_MPOLICY_AGE_CNT; i++)
        for (j = 0; j < HSE_MPOLICY_DTYPE_CNT; j++)
            for (k = 0; k < HSE_MPOLICY_MEDIA_CNT; k++) {
                enum hse_mclass_policy_media hse_mtype;
                enum mp_media_classp         mpool_mtype;

                hse_mtype = policies[4].mc_table[i][j][k];

                if (!((i == HSE_MPOLICY_AGE_INTERNAL) && (j == HSE_MPOLICY_DTYPE_VALUE))) {
                    /* Media type should match staging_capacity_nofallback */
                    ASSERT_EQ(hse_mtype, dpolicies[2].mc_table[i][j][k]);
                } else if (k == 0) {
                    /* <internal, leaf> first preference is capacity */
                    ASSERT_EQ(hse_mtype, HSE_MPOLICY_MEDIA_CAPACITY);
                } else {
                    /* <internal, leaf> no second preference */
                    ASSERT_EQ(hse_mtype, HSE_MPOLICY_MEDIA_INVALID);
                }

                mpool_mtype = mclass_policy_get_type(&policies[4], i, j, k);
                if (hse_mtype == HSE_MPOLICY_MEDIA_INVALID)
                    ASSERT_EQ(mpool_mtype, MP_MED_INVALID);
                else if (hse_mtype == HSE_MPOLICY_MEDIA_STAGING)
                    ASSERT_EQ(mpool_mtype, MP_MED_STAGING);
                else
                    ASSERT_EQ(mpool_mtype, MP_MED_CAPACITY);
            }

    hse_params_destroy(params);
}

MTF_END_UTEST_COLLECTION(mclass_policy_test);
