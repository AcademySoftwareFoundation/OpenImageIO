#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os
import shutil
import OpenImageIO as oiio

# Test OpenEXR's collection of damaged and broken images, to be sure
# we detect the damage and issue errors (not crash, etc).

failureok = True

# Save error output, too, and skip lines to make readable
redirect = ' >> out.txt 2>&1 '

# Figure out the version of openexr
openexr_version = 0
liblist = oiio.get_string_attribute("library_list").split(';')
for lib in liblist :
    lib_ver = lib.split(':')
    if lib_ver[0] == 'openexr' :
        ver = lib_ver[1].split(' ')[-1].split('.')
        openexr_version = int(ver[0]) * 10000 + int(ver[1]) * 100 + int(ver[2])
        print (openexr_version)


# ../openexr-images/Damaged:
# README   t03.exr  t06.exr  t09.exr  t12.exr  t15.exr
# t01.exr  t04.exr  t07.exr  t10.exr  t13.exr  t16.exr
# t02.exr  t05.exr  t08.exr  t11.exr  t14.exr
imagedir = OIIO_TESTSUITE_IMAGEDIR + "/Damaged"
files = [
"asan_heap-oob_4cb169_255_cc7ac9cde4b8634b31cb41c8fe89b92d_exr",
"asan_heap-oob_504235_131_78b0e15388401193e1ee2ce20fb7f3b0_exr",
"asan_heap-oob_7efd58fcbf3d_356_668b128c27c62e0d8314c503831fde88_exr",
"asan_heap-oob_7efd9bd346a5_639_9e0b30ed499cdf9e8802dd64e16a9508_exr",
"asan_heap-oob_7f09128c0ec1_358_ecaca7fb3f230d9d842074e1dd88f29b_exr",
"asan_heap-oob_7f0faa6bb393_900_7d9ed0a6eaa68f8308a042d725119ad2_exr",
"asan_heap-oob_7f11c0330393_935_240e7cacd61711daf4285366fea95e0c_exr",
"asan_heap-oob_7f11ece681f1_785_a570a0a25ada4752dabb554ad6b1eb6b_exr",
"asan_heap-oob_7f171b7ab3a2_937_b4e2415c399c2ab39548de911223769d_exr",
"asan_heap-oob_7f178ac80539_109_519f88e9ededfff61f535d6c9eb25a85_exr",
"asan_heap-oob_7f1fd65113ac_935_8fd55930e544dc3fb88659a6a8509c14_exr",
"asan_heap-oob_7f35311a1426_780_4871d40882e0fe7fae1427a82319e144_exr",
"asan_heap-oob_7f479f9536bd_391_5953693841a7931caf3d4592f8b9c90b_exr",
"asan_heap-oob_7f4aaebde389_918_a40f029a8121e5e26fe338b1fb91846e_exr",
"asan_heap-oob_7f4d5072b39d_561_5f5e4ef49a581edaf7bf0858fbfcfdd1_exr",
"asan_heap-oob_7f4f5558d00e_414_ec6445a8638a21c93ce8feb5a2e71981_exr",
"asan_heap-oob_7f54ed80df53_321_19490ab1841d3854eec557f3c23d0db0_exr",
"asan_heap-oob_7f5860f8d1f4_188_2e62008f8ecb3bb2ed67c09fa0939da7_exr",
"asan_heap-oob_7f58e8d75e8b_186_4bb7b1de93e9e44ea921c301b29a8026_exr",
"asan_heap-oob_7f5a5030cdca_702_045a76649e773c9c18b706c9853f18d9_exr",
"asan_heap-oob_7f5c18182f27_369_1fb4dae7654c77cd79646d3aa049d5dd_exr",
"asan_heap-oob_7f5cd523238e_878_03c277ec5021331fb301c7c1caa7dfd8_exr",
"asan_heap-oob_7f5cdab9a3a7_415_c33b838f08aafc976d3c24139936e122_exr",
"asan_heap-oob_7f6798416389_229_18bd946a4fde157b9974d16a51a4851d_exr",
"asan_heap-oob_7f6a78657cbf_916_10cc35387b54fdc7744f91b5bb424009_exr",
"asan_heap-oob_7f6e5e983398_203_ff7c0c73c79483db1fee001d77463c37_exr",
"asan_heap-oob_7f6efa7c53a7_991_4f9bd6fda4f5ae991775244b4945a7fb_exr",
"asan_heap-oob_7f6f881fa398_561_20ecb7f5a431d03a1502c28cab1214ad_exr",
"asan_heap-oob_7f730474b07c_543_fb506af38c88894d92ba0d433cf41abc_exr",
"asan_heap-oob_7f76b2c2cefb_196_ea4f8db8b4f2c11e02428c08e9bbbbb8_exr",
"asan_heap-oob_7f7a75f9abf5_577_a2b8668cc6069643543cb80fedca3ee4_exr",
"asan_heap-oob_7f8170c1abfa_115_8c6e33969541bf432ef7e68cc369728c_exr",
"asan_heap-oob_7f8a69d8339d_829_381ccc69dc6bd21c43a1deb0965bf5ab_exr",
"asan_heap-oob_7f8dd48421cd_321_7bae35650e908b12dbee1cf01e3d357f_exr",
"asan_heap-oob_7f8ed39ceed3_955_c6bb655a1bbfab9c5b511bd2b767e023_exr",
"asan_heap-oob_7f9acb068ee5_177_ec645ad270202d39ba5e80c826bbf13d_exr",
"asan_heap-oob_7f9cc08a96a5_942_e708072e479264a7808c055094a0fed9_exr",
"asan_heap-oob_7fa0e1f48cbf_760_be9901248390240a24449d4e8a97f6f2_exr",
"asan_heap-oob_7fa34eacd389_820_476a8109ebb3f7d02252e773b7bca45d_exr",
"asan_heap-oob_7faf9aba03ac_414_75af58c21b9b9e994747f9d6a5fc46d4_exr",
"asan_heap-oob_7fb3a7c6fc99_871_52d1f03c515bc91cc894515beea56a4f_exr",
"asan_heap-oob_7fb97d097381_293_78e73b6494a955e87faabcb16b35faa0_exr",
"asan_heap-oob_7fbe2d8e838e_932_9e9d2b0a870c0ad516189d274c2f98e4_exr",
"asan_heap-oob_7fc6b05f1eaf_255_e967badfa747d1bb0eda71a1883b419e_exr",
"asan_heap-oob_7fca10855564_529_6d418eae3e33a819185a8b09c40fd123_exr",
"asan_heap-oob_7fcdcb0a9f65_100_70d0d5b98567a5174d512dba7a603377_exr",
"asan_heap-oob_7fce901e7498_737_927b67c9a1ecd5f997d3a2620fdbf639_exr",
"asan_heap-oob_7fd328d0d206_535_9cc3d65c368fb138cb6a4bdd4da8070f_exr",
"asan_heap-oob_7fd921b41f11_369_da245dc772c0a5a60ce7b759b2132c51_exr",
"asan_heap-oob_7fdb9de0b38e_829_636ff2831c664e14a73282a3299781dd_exr",
"asan_heap-oob_7fe4c4caa975_277_153f78ec07237d01e8902143da28c7ec_exr",
"asan_heap-oob_7fe667eb33a2_999_31f64961e47968656f00573f7db5c67d_exr",
"asan_heap-oob_7fe814d1ce9d_617_e00af1c4c76b988122b68d43b073dd47_exr",
"asan_heap-oob_7ff4b88b21df_560_e119e0ba5bf9345e7daa85cc2fff6684_exr",
"asan_stack-oob_433d4f_157_4c56ecca982bc10e976527cd314bbcfa_exr",
"asan_stack-oob_433d4f_436_bb29e6f88ad5f5b2f5f9b68a3655b1d8_exr",
"asan_stack-oob_433d4f_510_f8d3ef49bd6e5f7ca4fb7e0f9c0af139_exr",
"openexr_2.2.0_heap_buffer_overflow_exr",
"openexr_2.2.0_memory_allocation_error_1_exr",
# "openexr_2.2.0_memory_allocation_error_2_exr",
"openexr_2.2.0_memory_allocation_error_3_exr",
"poc-099125d15685ef30feae813ddae15406b3f539d7cc4aa399176a50dcfe9be95c",
"poc-2b68475a090117f033df54c2de31546f7a1927ecadd9d0aa9b6bb8daad8ea971_min",
"poc-3e54bd90fc0e2a0b348ecd80d96738ed8324de388b3e88fd6c5fc666c2f01d83_min",
"poc-4d912f49ddc13ff49f95543880d47c85a8918e563fb723c340264f1719057613.mini",
"poc-66e4d1f68a3112b9aaa93831afbf8e283fd39be5e4591708bad917e6886c0ebb.mini",
"poc-89f7735a7cc9dcee59bfce4da70ad12e35a8809945b01b975d1a52ec15dbeccc",
"poc-9651abd6ee953b9f669db5927f8832f1b1eab028fa6ae7b4176a701aeea0ec90",
"poc-a905d63836959293bed720ab7d242bd07b7b7ec81ee3ee1e14e89853344dafbf_min",
"poc-af451a11e18ad9ca6ddc098bfd8b42f803bec2be8fafa6e648b8a7adcfdd0c06_min",
"poc-bd9579c640a6ee867d140c2a4d3bbd6f0452d4726f3f25ed53bf666f558ed245_min",
"poc-c9457552c1c04ea0f98154bc433a5f5d0421a7e705e6f396aba4339568473435_min",
"poc-cbc6ff03d6bc31f0c61476525641852b0220278e6576a651029c50e86f7f0c77",
"poc-d545cd0db4595a1c05504ab53d83cc8c6fce02387545aa49e979ee087c1ddf8f_min",
"poc-df1fefc5fb459cb12183eae34dc696cd0e77b0b8deb4cd1ef3dc25279b7a6bde_min",
"poc-e2106eebb303e685cee66860c931fe1a4eb9af1a7f5bef5b3b95f90c3e8ae0e0_min",
"poc-fb9238840f4d9d93ab3ac989b70000f9795ab6ad399bff00093b944e6a893403_min",
]

if openexr_version >= 20600 :
    files += [
        "signal_sigsegv_7ffff7b21e8a_389_bf048bf41ca71b4e00d2b0edd0a39e27_exr",
    ]


# Need to copy the files to a temp location to add a true .exr suffix
if not os.path.exists('tmpsrc'):
    os.mkdir('tmpsrc')



for f in files:
    # First, do a simple read/write loop to test ordinary ImageInput
    shutil.copyfile(os.path.join(imagedir, f),
                    os.path.join('tmpsrc', f+'.exr'))
    command += oiiotool ('-n -info tmpsrc/'+f+'.exr -o out.exr')
    command += '; echo >> out.txt \n'
