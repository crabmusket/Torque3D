<?php
if( inProjectConfig() )  
{  
  includeLib( 'assimp' );
  addProjectDefine( 'TORQUE_ASSIMP' );
  addEngineSrcDir( 'ts/assimp' );
  addLibIncludePath( 'assimp/include' );
  addProjectDependency( 'assimp' );  
  addSolutionProjectRef( 'assimp' );  
} 
?>
