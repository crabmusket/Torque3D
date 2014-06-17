new GuiControlProfile(GuiDefaultProfile);
new GuiControlProfile(GuiToolTipProfile);
new GuiCanvas(Canvas);
Canvas.setWindowTitle("Torque 3D Unit Tests");
new RenderPassManager(DiffuseRenderPassManager);
setLightManager("Basic Lighting");
$success = unitTest_runTests("", true);
quit($success ? 0 : 1);
