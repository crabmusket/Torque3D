<?php
beginModule( 'assimp' );

	// Collada Loader
	addProjectDefine( 'TORQUE_ASSIMP' );
    addProjectDefine( 'ASSIMP_BUILD_BOOST_WORKAROUND' );

	addEngineSrcDir( 'ts/assimp' );

	addLibIncludePath( 'assimp/include' );

endModule();
?>
