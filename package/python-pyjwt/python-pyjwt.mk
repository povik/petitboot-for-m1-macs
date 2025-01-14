################################################################################
#
# python-pyjwt
#
################################################################################

PYTHON_PYJWT_VERSION = 2.2.0
PYTHON_PYJWT_SOURCE = PyJWT-$(PYTHON_PYJWT_VERSION).tar.gz
PYTHON_PYJWT_SITE = https://files.pythonhosted.org/packages/b9/6d/e5ac4eb80724cb4201de6dcbc1e8a9f5cb65a0424b30c4c69e49bc1363d1
PYTHON_PYJWT_SETUP_TYPE = setuptools
PYTHON_PYJWT_LICENSE = MIT
PYTHON_PYJWT_LICENSE_FILES = LICENSE
PYTHON_PYJWT_CPE_ID_VENDOR = pyjwt_project
PYTHON_PYJWT_CPE_ID_PRODUCT = pyjwt

$(eval $(python-package))
