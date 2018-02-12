#!/usr/bin/env python

"""
env
PYTHONPATH=/net/soft_scratch/users/jeremys/git/ocio.js/build/src/pyglue/
LD_LIBRARY_PATH=/net/soft_scratch/users/jeremys/git/ocio.js/build/src/core/
"""

import math, os, sys
import PyOpenColorIO as OCIO

print "OCIO",OCIO.version

outputfilename = "config.ocio"

def WriteSPI1D(filename, fromMin, fromMax, data):
    f = file(filename,'w')
    f.write("Version 1\n")
    f.write("From %s %s\n" % (fromMin, fromMax))
    f.write("Length %d\n" % len(data))
    f.write("Components 1\n")
    f.write("{\n")
    for value in data:
        f.write("        %s\n" % value)
    f.write("}\n")
    f.close()

def Fit(value, fromMin, fromMax, toMin, toMax):
    if fromMin == fromMax:
        raise ValueError("fromMin == fromMax")
    return (value - fromMin) / (fromMax - fromMin) * (toMax - toMin) + toMin


###############################################################################


config = OCIO.Config()
config.setSearchPath('luts')

config.setRole(OCIO.Constants.ROLE_SCENE_LINEAR, "linear")
config.setRole(OCIO.Constants.ROLE_REFERENCE, "linear")
config.setRole(OCIO.Constants.ROLE_COLOR_TIMING, "Cineon")
config.setRole(OCIO.Constants.ROLE_COMPOSITING_LOG, "Cineon")
config.setRole(OCIO.Constants.ROLE_DATA,"raw")
config.setRole(OCIO.Constants.ROLE_DEFAULT,"raw")
config.setRole(OCIO.Constants.ROLE_COLOR_PICKING,"sRGB")
config.setRole(OCIO.Constants.ROLE_MATTE_PAINT,"sRGB")
config.setRole(OCIO.Constants.ROLE_TEXTURE_PAINT,"sRGB")


###############################################################################

cs = OCIO.ColorSpace(name='linear')
cs.setDescription("Scene-linear, high dynamic range. Used for rendering and compositing.")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_LG2)
cs.setAllocationVars([-15.0, 6.0])
config.addColorSpace(cs)


###############################################################################

def toSRGB(v):
    if v<0.04045/12.92:
        return v*12.92
    return 1.055 * v**(1.0/2.4) - 0.055

def fromSRGB(v):
    if v<0.04045:
        return v/12.92
    return ((v + .055) / 1.055) ** 2.4

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromSRGB(x))

# Data is srgb->linear
WriteSPI1D('luts/srgb.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='sRGB')
cs.setDescription("Standard RGB Display Space")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('srgb.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


NUM_SAMPLES = 2**16+25
RANGE = (-0.125, 4.875)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromSRGB(x))

# Data is srgb->linear
WriteSPI1D('luts/srgbf.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='sRGBf')
cs.setDescription("Standard RGB Display Space, but with additional range to preserve float highlights.")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('srgbf.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################

def toRec709(v):
    if v<0.018:
        return v*4.5
    return 1.099 * v**0.45 - 0.099

def fromRec709(v):
    if v<0.018*4.5:
        return v/4.5
    return ((v + .099) / 1.099) ** (1.0/0.45)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromRec709(x))

# Data is srgb->linear
WriteSPI1D('luts/rec709.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='rec709')
cs.setDescription("Rec. 709 (Full Range) Display Space")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('rec709.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################

cineonBlackOffset = 10.0 ** ((95.0 - 685.0)/300.0)

def fromCineon(x):
    return (10.0**((1023.0 * x - 685.0) / 300.0) - cineonBlackOffset) / (1.0 - cineonBlackOffset)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromCineon(x))

# Data is srgb->linear
WriteSPI1D('luts/cineon.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='Cineon')
cs.setDescription("Cineon (Log Film Scan)")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('cineon.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)



###############################################################################

cs = OCIO.ColorSpace(name='Gamma1.8')
cs.setDescription("Emulates a idealized Gamma 1.8 display device.")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([0.0, 1.0])

t = OCIO.ExponentTransform(value=(1.8,1.8,1.8,1.0))
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)

cs = OCIO.ColorSpace(name='Gamma2.2')
cs.setDescription("Emulates a idealized Gamma 2.2 display device.")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([0.0, 1.0])

t = OCIO.ExponentTransform(value=(2.2,2.2,2.2,1.0))
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################


# Log to Linear light conversions for Panalog
# WARNING: these are estimations known to be close enough.
# The actual transfer functions are not published

panalogBlackOffset = 10.0 ** ((64.0 - 681.0) / 444.0)

def fromPanalog(x):
    return (10.0**((1023 * x - 681.0) / 444.0) - panalogBlackOffset) / (1.0 - panalogBlackOffset)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromPanalog(x))

# Data is srgb->linear
WriteSPI1D('luts/panalog.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='Panalog')
cs.setDescription("Sony/Panavision Genesis Log Space")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('panalog.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)



###############################################################################



redBlackOffset = 10.0 ** ((0.0 - 1023.0) / 511.0)

def fromREDLog(x):
    return ((10.0 ** ((1023.0 * x - 1023.0) / 511.0)) - redBlackOffset) / (1.0 - redBlackOffset)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings


NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromREDLog(x))

# Data is srgb->linear
WriteSPI1D('luts/redlog.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='REDLog')
cs.setDescription("RED Log Space")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('redlog.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)



###############################################################################

def fromViperLog(x):
    return 10.0**((1023.0 * x - 1023.0) / 500.0)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings


NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromViperLog(x))

# Data is srgb->linear
WriteSPI1D('luts/viperlog.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='ViperLog')
cs.setDescription("Viper Log Space")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('viperlog.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)



###############################################################################


alexav3logc_a = 5.555556
alexav3logc_b = 0.052272
alexav3logc_c = 0.247190
alexav3logc_d = 0.385537
alexav3logc_e = 5.367655
alexav3logc_f = 0.092809
alexav3logc_cut = 0.010591
alexav3logc_eCutF = alexav3logc_e*alexav3logc_cut + alexav3logc_f

# This corresponds to EI800 per Arri Doc
# http://www.arridigital.com/forum/index.php?topic=6372.0
# http://www.arri.com/?eID=registration&file_uid=7775
def fromAlexaV3LogC(x):
    if x > alexav3logc_eCutF:
        return (10.0 **((x - alexav3logc_d) / alexav3logc_c) - alexav3logc_b) / alexav3logc_a
    else:
        return (x - alexav3logc_f) / alexav3logc_e


# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings


NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromAlexaV3LogC(x))

# Data is srgb->linear
WriteSPI1D('luts/alexalogc.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='AlexaV3LogC')
cs.setDescription("Alexa Log C")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('alexalogc.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################

'PLogLin'

# Josh Pines style pivoted log/lin conversion
minLinValue = 1e-10
linReference = 0.18
logReference = 445.0
negativeGamma = 0.6
densityPerCodeValue = 0.002
ngOverDpcv = negativeGamma/densityPerCodeValue
dpcvOverNg = densityPerCodeValue/negativeGamma

def fromPLogLin(x):
    return (10.0**((x*1023.0 - logReference)*dpcvOverNg ) * linReference)


# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromPLogLin(x))

# Data is srgb->linear
WriteSPI1D('luts/ploglin.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='PLogLin')
cs.setDescription("Josh Pines style pivoted log/lin conversion. 445->0.18")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('ploglin.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################

def fromSLog(x):
    return (10.0 ** (((x - 0.616596 - 0.03) / 0.432699)) - 0.037584)

# These samples and range have been chosen to write out this colorspace with
# a limited over/undershoot range, which also exactly samples the 0.0,1.0
# crossings

NUM_SAMPLES = 2**12+5
RANGE = (-0.125, 1.125)
data = []
for i in xrange(NUM_SAMPLES):
    x = i/(NUM_SAMPLES-1.0)
    x = Fit(x, 0.0, 1.0, RANGE[0], RANGE[1])
    data.append(fromSLog(x))

# Data is srgb->linear
WriteSPI1D('luts/slog.spi1d', RANGE[0], RANGE[1], data)

cs = OCIO.ColorSpace(name='SLog')
cs.setDescription("Sony SLog")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setAllocation(OCIO.Constants.ALLOCATION_UNIFORM)
cs.setAllocationVars([RANGE[0], RANGE[1]])

t = OCIO.FileTransform('slog.spi1d', interpolation=OCIO.Constants.INTERP_LINEAR)
cs.setTransform(t, OCIO.Constants.COLORSPACE_DIR_TO_REFERENCE)
config.addColorSpace(cs)


###############################################################################

'REDSpace'



###############################################################################

cs = OCIO.ColorSpace(name='raw')
cs.setDescription("Raw Data. Used for normals, points, etc.")
cs.setBitDepth(OCIO.Constants.BIT_DEPTH_F32)
cs.setIsData(True)
config.addColorSpace(cs)


###############################################################################

display = 'default'
config.addDisplay(display, 'None', 'raw')
config.addDisplay(display, 'sRGB', 'sRGB')
config.addDisplay(display, 'rec709', 'rec709')

config.setActiveDisplays('default')
config.setActiveViews('sRGB')


###############################################################################



try:
    config.sanityCheck()
except Exception,e:
    print e

f = file(outputfilename,"w")
f.write(config.serialize())
f.close()
print "Wrote",outputfilename

# Core/LUT/include/LUT/fnLUTConversions.h

