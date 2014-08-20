function BehaviorFieldStack::createMaterialProperty(%this, %accessor, %filter, %label, %tooltip, %data)
{
   %extent = 64;
      
   %container = new GuiControl() {
      canSaveDynamicFields = "0";
      Profile = "EditorContainerProfile";
      HorizSizing = "right";
      VertSizing = "bottom";
      Position = "0 0";
      Extent = "300 89";
      MinExtent = "8 2";
      canSave = "0";
      Visible = "1";
      hovertime = "100";
      tooltip = %tooltip;
      tooltipProfile = "EditorToolTipProfile";
   };

   %labelControl = new GuiTextCtrl() {
      canSaveDynamicFields = "0";
      Profile = "EditorFontHLBold";
      HorizSizing = "right";
      VertSizing = "bottom";
      Position = "16 3";
      Extent = "100 18";
      MinExtent = "8 2";
      canSave = "0";
      Visible = "1";
      hovertime = "100";
      tooltip = %tooltip;
      tooltipProfile = "EditorToolTipProfile";
      text = %label;
      maxLength = "1024";
   };

   //
   %imageContainer = new GuiControl(){
      profile = "ToolsGuiDefaultProfile";
      Position = "0 0";
      Extent = "74 87";
      HorizSizing = "right";
      VertSizing = "bottom";
      isContainer = "1";
      
      new GuiTextCtrl(){
         position = "7 71";
         profile = "ToolsGuiTextCenterProfile";
         extent = "64 16";
         text = %matName;
      };
   };
   
   %previewButton = new GuiBitmapButtonCtrl(){
      internalName = %matName;
      HorizSizing = "right";
      VertSizing = "bottom";
      profile = "ToolsGuiButtonProfile";
      position = "7 4";
      extent = "64 64";
      buttonType = "PushButton";
      bitmap = "";
      Command = "";
      text = "Loading...";
      useStates = false;
      
      new GuiBitmapButtonCtrl(){
            HorizSizing = "right";
            VertSizing = "bottom";
            profile = "ToolsGuiButtonProfile";
            position = "0 0";
            extent = "64 64";
            Variable = "";
            buttonType = "toggleButton";
            bitmap = "tools/materialEditor/gui/cubemapBtnBorder";
            groupNum = "0";
            text = "";
         }; 
   }; 
   
   %previewBorder = new GuiButtonCtrl(){
         internalName = %matName@"Border";
         HorizSizing = "right";
         VertSizing = "bottom";
         profile = "ToolsGuiThumbHighlightButtonProfile";
         position = "3 0";
         extent = "72 88";
         Variable = "";
         buttonType = "toggleButton";
         tooltip = %matName;
         Command = "MaterialSelector.updateSelection( $ThisControl.getParent().getObject(1).internalName, $ThisControl.getParent().getObject(1).bitmap );"; 
         groupNum = "0";
         text = "";
   };
   
   %imageContainer.add(%previewButton);  
   %imageContainer.add(%previewBorder);
   %container.add(%imageContainer);	
   //
   
   %editControl = new GuiTextEditCtrl() {
      class = "BehaviorEdTextField";
      internalName = %accessor @ "File";
      canSaveDynamicFields = "0";
      Profile = "EditorTextEdit";
      HorizSizing = "right";
      VertSizing = "bottom";
      Position = "100 1";
      Extent = %extent - 17 SPC "22";
      MinExtent = "8 2";
      canSave = "0";
      Visible = "1";
      hovertime = "100";
      tooltip = %tooltip;
      tooltipProfile = "EditorToolTipProfile";
      maxLength = "1024";
      historySize = "0";
      password = "0";
      text = %data;
      
      tabComplete = "0";
      sinkAllKeyEvents = "0";
      password = "0";
      passwordMask = "*";
      precision = %precision;
      accessor = %accessor;
      isProperty = true;
      undoLabel = %label;
      object = %this.object;
      useWords = false;
   };
   
   %browse = new GuiButtonCtrl() {
      canSaveDynamicFields = "0";
      className = "materialFieldBtn";
      Profile = "GuiButtonProfile";
      HorizSizing = "right";
      VertSizing = "bottom";
      Position = %editControl.position.x + %extent - 17 SPC "1";
      Extent = "15 22";
      MinExtent = "8 2";
      canSave = "0";
      Visible = "1";
      hovertime = "100";
      tooltip = "Browse for a file";
      tooltipProfile = "EditorToolTipProfile";
      text = "...";
      pathField = %editControl;
   };
   %browse.lastPath = %editControl.text;
   
   if(%filter $= "models")
      %browse.filter = $ModelFieldTypes;
   else if(%filter $= "images")
      %browse.filter = $ImageFieldTypes;
   else if(%filter $= "sounds")
      %browse.filter = $SoundFieldTypes;
   else
      %browse.filter = "All Files|*.*";

   %container.add(%labelControl);
   %container.add(%editControl);
   %container.add(%browse);
   
   return %container;
}

function materialFieldBtn::onClick( %this )
{
   if(%this.lastPath $= "")
      %this.lastPath = "art";
    
   getLoadFilename( %this.filter, %this @ ".onBrowseSelect", %this.lastPath );
}

function materialFieldBtn::onBrowseSelect( %this, %path )
{
   %path = makeRelativePath( %path, getMainDotCSDir() );
   %this.lastPath = %path;
   %this.pathField.text = %path;
   %this.pathField.onReturn(); //force the update
   %this.object.inspectorApply();
}