from distutils.core import setup, Extension

quota = Extension('quota', 
	sources = ['quotamodule.c', 'bylabel.c', 'common.c', 'quot.c', 'quotasys.c',
		'quotaio_rpc.c','quotaio_v1.c', 'quotaio_v2.c', 'quotaio_xfs.c', 'quotaio.c','quotaio_meta.c','quotaio_generic.c',
		'quotaops.c',
		'rquota_client.c','rquota_clnt.c', 'rquota_xdr.c'
	],
	define_macros = [('RPC',None),('QUOTA_VERSION',3.17),('COMPILE_OPTS',None),('BSD_BEHAVIOUR',None)],
	libraries = ['m'],
)

setup (name = 'Quota',
	version = '0.1',
	description = 'Python User Quota Information',
	author = 'Thomas Uphill',
	author_email='uphill@ias.edu',
	url='http://puias.math.ias.edu/python-quota',
	ext_modules = [quota])
