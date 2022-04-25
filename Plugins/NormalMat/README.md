NormalMat is a normal map shader that gives you the ability to add details to your geo using an image. 

Use output channel if you want to output the normal from the scanline renderer, the build in option inside the scanline render doesn't work with this tool.
Other than that the tool works with the rest of the native lighs for the scanline renderer including the enviroment light. 

There are two option for image input range:
* from 0 to 1, the plugin will shift it the range of -1 to 1
* from -1 to 1, in thise case nothing will be done.

The flip check box is just to "fix" so normal that start to band too much. 

<div id="header" align="left">
  <img src="https://github.com/EyalShirazi/Nuke/blob/main/Plugins/NormalMat/demo/NormalMat%20-%20example02.gif"/>
</div>

<div id="header" align="left">
  <img src="https://github.com/EyalShirazi/Nuke/blob/main/Plugins/NormalMat/demo/NormalMat%20-%20example01.gif"/>
</div>


