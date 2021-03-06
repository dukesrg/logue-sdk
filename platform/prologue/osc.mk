# #############################################################################
# Prologue Oscillator Customization
# #############################################################################

PKGEXT = prlgunit

MCSRC = osc_unit.c

MLDSCRIPT = userosc.ld

MDEFS = -DSTM32F401xC -DUSER_TARGET_PLATFORM=k_user_target_prologue -DUSER_TARGET_MODULE=k_user_module_osc

MSYMS = osc_api.syms
