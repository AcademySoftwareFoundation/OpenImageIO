# unit tests for TypeDesc

import OpenImageIO as oiio
import array

def td_basetype_test():
    print "Starting TypeDesc::BASETYPE enum tests..."
    # test 1
    try:
        oiio.BASETYPE.UNKNOWN
        print "Test 1 passed"
    except:
        print "Test 1 failed"
    # test 2
    try:
        oiio.BASETYPE.NONE
        print "Test 2 passed"
    except:
        print "Test 2 failed"
    # Test 3
    try:
        oiio.BASETYPE.UCHAR
        print "Test 3 passed"
    except:
        print "Test 3 failed"
    # Test 4
    try:
        oiio.BASETYPE.UINT8
        print "Test 4 passed"
    except:
        print "Test 4 failed"
    # Test 5
    try:
        oiio.BASETYPE.CHAR
        print "Test 5 passed"
    except:
        print "Test 5 failed"
    # Test 6
    try:
        oiio.BASETYPE.INT8
        print "Test 6 passed"
    except:
        print "Test 6 failed"
    # Test 7
    try:
        oiio.BASETYPE.USHORT
        print "Test 7 passed"
    except:
        print "Test 8 failed"
    # Test 8
    try:
        oiio.BASETYPE.UINT16
        print "Test 9 passed"
    except:
        print "Test 9 failed"
    # Test 10
    try:
        oiio.BASETYPE.SHORT
        print "Test 10 passed"
    except:
        print "Test 10 failed"
    # Test 11
    try:
        oiio.BASETYPE.INT16
        print "Test 11 passed"
    except:
        print "Test 11 failed"
    # Test 12
    try:
        oiio.BASETYPE.UINT
        print "Test 12 passed"
    except:
        print "Test 12 failed"
    # Test 13
    try:
        oiio.BASETYPE.INT
        print "Test 13 passed"
    except:
        print "Test 13 failed"
    # Test 14
    try:
        oiio.BASETYPE.HALF
        print "Test 14 passed"
    except:
        print "Test 14 failed"
    # Test 15
    try:
        oiio.BASETYPE.FLOAT
        print "Test 15 passed"
    except:
        print "Test 15 failed"
    # Test 16
    try:
        oiio.BASETYPE.DOUBLE
        print "Test 16 passed"
    except:
        print "Test 16 failed"
    # Test 17
    try:
        oiio.BASETYPE.STRING
        print "Test 17 passed"
    except:
        print "Test 17 failed"
    # Test 18
    try:
        oiio.BASETYPE.PTR
        print "Test 18 passed"
    except:
        print "Test 18 failed"
    # Test 19
    try:
        oiio.BASETYPE.LASTBASE
        print "Test 19 passed"
    except:
        print "Test 19 failed"

    print


def td_aggregate_test():
    print "Running TypeDesc::AGGREGATE enum tests..."
    # Test 1
    try:
        oiio.AGGREGATE.SCALAR
        print "Test 1 passed"
    except:
        print "Test 1 failed"
    # Test 2
    try:
        oiio.AGGREGATE.SCALAR
        print "Test 2 passed"
    except:
        print "Test 2 failed"
    # Test 3
    try:
        oiio.AGGREGATE.SCALAR
        print "Test 3 passed"
    except:
        print "Test 3 failed"
    # Test 4
    try:
        oiio.AGGREGATE.SCALAR
        print "Test 4 passed"
    except:
        print "Test 4 failed"
    # Test 5
    try:
        oiio.AGGREGATE.SCALAR
        print "Test 5 passed"
    except:
        print "Test 5 failed"
    
    print


def td_vecsemantics_test():
    print "Running TypeDesc::AGGREGATE enum tests..."
    # Test 1
    try:
        oiio.VECSEMANTICS.NOXFORM
        print "Test 1 passed"
    except:
        print "Test 1 failed"
    # Test 2
    try:
        oiio.VECSEMANTICS.COLOR
        print "Test 2 passed"
    except:
        print "Test 2 failed"
    # Test 3
    try:
        oiio.VECSEMANTICS.POINT
        print "Test 3 passed"
    except:
        print "Test 3 failed"
    # Test 4
    try:
        oiio.VECSEMANTICS.VECTOR
        print "Test 4 passed"
    except:
        print "Test 4 failed"
    # Test 5
    try:
        oiio.VECSEMANTICS.NORMAL
        print "Test 5 passed"
    except:
        print "Test 5 failed"
    
    print


def td_data_members_test():
    print "Starting TypeDesc data members tests..."
    desc = oiio.TypeDesc()
    # test 1
    if  desc.basetype == 0:
        print "Test 1 passed"
    else:
        print "Test 1 failed"
    # test 2
    if desc.aggregate == 1:
        print "Test 2 passed"
    else:
        print "Test 2 failed"
    # test 3
    if desc.vecsemantics == 0:
        print "Test 3 passed"
    else:
        print "Test 3 failed"
    # test 4
    if desc.arraylen == 0:
        print "Test 4 passed"
    else:
        print "Test 4 failed"
    
    print


def run_tests():
    td_basetype_test()
    td_aggregate_test()
    td_vecsemantics_test()
    td_data_members_test()


run_tests()
