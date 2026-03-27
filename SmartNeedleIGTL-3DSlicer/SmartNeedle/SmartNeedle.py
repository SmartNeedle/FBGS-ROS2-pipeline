import logging
import os
import vtk
import slicer
from slicer.i18n import tr as _
from slicer.i18n import translate
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin
from slicer.parameterNodeWrapper import parameterNodeWrapper
from slicer import vtkMRMLMarkupsFiducialNode

import CurveMaker

#
# SmartNeedle
#


class SmartNeedle(ScriptedLoadableModule):
    """Uses ScriptedLoadableModule base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = _("SmartNeedle")  # TODO: make this more human readable by adding spaces
        # TODO: set categories (folders where the module shows up in the module selector)
        self.parent.categories = [translate("qSlicerAbstractCoreModule", "IGT")]
        self.parent.dependencies = ["OpenIGTLinkIF", "CurveMaker"]
        self.parent.contributors = ["Mariana Bernardes (BWH)"] 
        # TODO: update with short description of the module and a link to online module documentation
        # _() function marks text as translatable to other languages
        self.parent.helpText = _("""
Connects 3D Slicer to the ROS 2 OpenIGTLink bridge and visualizes the
incoming SmartNeedle centerline in real time.
""")
        # TODO: replace with organization, grant and thanks
        self.parent.acknowledgementText = _("""
This project was partially funded by NIH grants.
""")

        self._shapeUpdateActive = False
        self._needleShapeObserverID = None

#
# SmartNeedleParameterNode
#

@parameterNodeWrapper
class SmartNeedleParameterNode:
    """
    The parameters needed by the module.

    igtlConnector - selected OpenIGTLink connector node
    shapeUpdateActive - if shape update is currently activated
    """
    igtlConnector: slicer.vtkMRMLIGTLConnectorNode
    shapeUpdateActive: bool = False

#
# SmartNeedleWidget
#


class SmartNeedleWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
    """Uses ScriptedLoadableModuleWidget base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self, parent=None) -> None:
        """Called when the user opens the module the first time and the widget is initialized."""
        ScriptedLoadableModuleWidget.__init__(self, parent)
        VTKObservationMixin.__init__(self)  # needed for parameter node observation
        self.logic = None
        self._parameterNode = None
        self._parameterNodeGuiTag = None
        self._shapeUpdateActive = False
        self._needleShapeObserverID = None

    def setup(self) -> None:
        """Called when the user opens the module the first time and the widget is initialized."""
        ScriptedLoadableModuleWidget.setup(self)

        # Load widget from .ui file (created by Qt Designer).
        # Additional widgets can be instantiated manually and added to self.layout.
        uiWidget = slicer.util.loadUI(self.resourcePath("UI/SmartNeedle.ui"))
        self.layout.addWidget(uiWidget)
        self.ui = slicer.util.childWidgetVariables(uiWidget)

        # Set scene in MRML widgets. Make sure that in Qt designer the top-level qMRMLWidget's
        # "mrmlSceneChanged(vtkMRMLScene*)" signal in is connected to each MRML widget's.
        # "setMRMLScene(vtkMRMLScene*)" slot.
        uiWidget.setMRMLScene(slicer.mrmlScene)

        # Create logic class. Logic implements all computations that should be possible to run
        # in batch mode, without a graphical user interface.
        self.logic = SmartNeedleLogic()

        # Connections

        # These connections ensure that we update parameter node when scene is closed
        self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartCloseEvent, self.onSceneStartClose)
        self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onSceneEndClose)

        # Selectors and buttons
        self.ui.igtlConnectorSelector.connect("currentNodeChanged(vtkMRMLNode*)",self.onConnectorSelected)
        self.ui.applyButton.connect("clicked(bool)", self.onApplyButton)
        self.ui.copyButton.connect("clicked(bool)", self.onCopyButton)

        # Make sure parameter node is initialized (needed for module reload)
        self.initializeParameterNode()

        
    def cleanup(self) -> None:
        """Called when the application closes and the module widget is destroyed."""
        self.stopShapeUpdate()
        self.removeObservers()

    def enter(self) -> None:
        """Called each time the user opens this module."""
        # Make sure parameter node exists and observed
        self.initializeParameterNode()

    def exit(self) -> None:
        """Called each time the user opens a different module."""
        # Do not react to parameter node changes (GUI will be updated when the user enters into the module)
        self.setParameterNode(None)
        

    def onSceneStartClose(self, caller, event) -> None:
        """Called just before the scene is closed."""
        # Parameter node will be reset, do not use it anymore
        self.setParameterNode(None)

    def onSceneEndClose(self, caller, event) -> None:
        """Called just after the scene is closed."""
        # If this module is shown while the scene is closed then recreate a new parameter node immediately
        if self.parent.isEntered:
            self.initializeParameterNode()

    def initializeParameterNode(self) -> None:
        """Ensure parameter node exists and observed."""
        # Parameter node stores all user choices in parameter values, node selections, etc.
        # so that when the scene is saved and reloaded, these settings are restored.

        self.setParameterNode(self.logic.getParameterNode())


    def setParameterNode(self, inputParameterNode: SmartNeedleParameterNode | None) -> None:
        """
        Set and observe parameter node.
        Observation is needed because when the parameter node is changed then the GUI must be updated immediately.
        """
        if self._parameterNode:
            self._parameterNode.disconnectGui(self._parameterNodeGuiTag)
            self.removeObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self._checkCanApply)
        self._parameterNode = inputParameterNode
        if self._parameterNode:
            # Note: in the .ui file, a Qt dynamic property called "SlicerParameterName" is set on each
            # ui element that needs connection.
            self._parameterNodeGuiTag = self._parameterNode.connectGui(self.ui)
            self.addObserver(self._parameterNode, vtk.vtkCommand.ModifiedEvent, self._checkCanApply)
            self._checkCanApply()

    def _checkCanApply(self, caller=None, event=None) -> None:
        if not self._parameterNode:
            return
        if self._parameterNode.shapeUpdateActive:
            self.ui.igtlConnectorSelector.setEnabled(False)
            self.ui.applyButton.setEnabled(True)
            self.ui.applyButton.setText(_("Stop"))
            self.ui.applyButton.setToolTip(_("Stop receiving shape updates."))
            return
        self.ui.igtlConnectorSelector.setEnabled(True)
        if self._parameterNode.igtlConnector:
            self.ui.applyButton.setEnabled(True)
            self.ui.applyButton.setText(_("Start"))
            self.ui.applyButton.setToolTip(_("Start receiving shape updates from the selected OpenIGTLink connector."))
        else:
            self.ui.applyButton.setEnabled(False)
            self.ui.applyButton.setText(_("Start"))
            self.ui.applyButton.setToolTip(_("Select an OpenIGTLink connector."))

    def onConnectorSelected(self, node) -> None:
        if node is None:
            return
        if not node.IsA("vtkMRMLIGTLConnectorNode"):
            return
        self.logic.initializeConnectorDefaults(node)

    def onNeedleShapeChange(self, caller=None, event=None) -> None:
        needleShape = self.logic.getNeedleShape()
        self.ui.timeStampTextbox.setText(
            needleShape["timestamp"] if needleShape["timestamp"] is not None else "-- : -- : --.----"
        )
        self.ui.packageNumberTextbox.setText(
            str(needleShape["package_number"]) if needleShape["package_number"] is not None else "---"
        )
        self.ui.numberPointsTextbox.setText(
            str(needleShape["num_points"]) if needleShape["num_points"] is not None else "---"
        )
        tipCoordinates = needleShape["tip_coordinates"]
        if tipCoordinates is None:
            self.ui.needleTipTextbox.setText("(---, ---, ---)")
        else:
            self.ui.needleTipTextbox.setText(
                f"({tipCoordinates[0]:.2f}, {tipCoordinates[1]:.2f}, {tipCoordinates[2]:.2f})"
            )

    def onApplyButton(self) -> None:
        if not self._parameterNode.shapeUpdateActive:
            with slicer.util.tryWithErrorDisplay(_("Failed to start shape update."), waitCursor=True):
                connectorNode = self._parameterNode.igtlConnector
                self.logic.startShapeUpdate(connectorNode)
                text_modified_event = getattr(
                    slicer.vtkMRMLTextNode,
                    "TextModifiedEvent",
                    vtk.vtkCommand.ModifiedEvent,
                )
                self._needleShapeObserverID = self.logic.needleShapeHeaderNode.AddObserver(
                    text_modified_event,
                    self.onNeedleShapeChange,
                )
                self._parameterNode.shapeUpdateActive = True
                self._checkCanApply()
        else:
            with slicer.util.tryWithErrorDisplay(_("Failed to stop shape update."), waitCursor=True):
                self.stopShapeUpdate()

    def stopShapeUpdate(self) -> None:
        if self._needleShapeObserverID is not None and self.logic.needleShapeHeaderNode is not None:
            self.logic.needleShapeHeaderNode.RemoveObserver(self._needleShapeObserverID)
            self._needleShapeObserverID = None
        self.logic.stopShapeUpdate()
        if self._parameterNode:
            self._parameterNode.shapeUpdateActive = False
        self._checkCanApply()

    def onCopyButton(self) -> None:
        shapeName = self.ui.shapeNameTextbox.text.strip()
        if not shapeName:
            slicer.util.errorDisplay("Please enter an insertion name.")
            return
        with slicer.util.tryWithErrorDisplay(_("Failed to save needle shape copy."), waitCursor=True):
            copyShapePoints, copyShapeModel, copyShapeHeader = self.logic.copyShape(shapeName)
            print(f"Saved:\n{copyShapePoints.GetName()}\n{copyShapeHeader.GetName()}\n{copyShapeModel.GetName()}","Needle shape saved")

#
# SmartNeedleLogic
#


class SmartNeedleLogic(ScriptedLoadableModuleLogic):
    """This class should implement all the actual
    computation done by your module.  The interface
    should be such that other python code can import
    this class and make use of the functionality without
    requiring an instance of the Widget.
    Uses ScriptedLoadableModuleLogic base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def __init__(self) -> None:
        """Called when the logic class is instantiated. Can be used for initializing member variables."""
        ScriptedLoadableModuleLogic.__init__(self)

        self.activeConnectorNode = None

        # Create internal nodes
        self.needleShapePointsNode = slicer.util.getFirstNodeByName("NeedleShape", className="vtkMRMLMarkupsFiducialNode")
        if self.needleShapePointsNode is None:
            self.needleShapePointsNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", "NeedleShape")
        self.needleShapeHeaderNode = slicer.util.getFirstNodeByName("NeedleShapeHeader", className="vtkMRMLTextNode")
        if self.needleShapeHeaderNode is None:
            self.needleShapeHeaderNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLTextNode", "NeedleShapeHeader")

        # Create the Needle Model node
        self.needleModelNode = slicer.util.getFirstNodeByName('NeedleModel', className='vtkMRMLModelNode')
        if self.needleModelNode is None:
            self.needleModelNode = slicer.vtkMRMLModelNode()
            self.needleModelNode.SetName('NeedleModel')   
            self.needleModelNode.SetDisplayVisibility(True)
            slicer.mrmlScene.AddNode(self.needleModelNode)
        # Check for Needle Model display node
        #self.displayNeedleModel = self.needleModelNode.GetDisplayNode()
        #if self.displayNeedleModel is None:
        #    self.displayNeedleModel = slicer.vtkMRMLModelDisplayNode()
        #    self.needleModelNode.SetAndObserveDisplayNodeID(self.displayNeedleModel.GetID())
        #self.displayNeedleModel.SetVisibility(True)
        #self.displayNeedleModel.SetVisibility3D(True)
        #self.displayNeedleModel.SetVisibility2D(True)
        # Get CurveMaker module logic
        self.curveMaker = CurveMaker.CurveMakerLogic()
        self.curveMaker.SourceNode = self.needleShapePointsNode
        self.curveMaker.DestinationNode = self.needleModelNode
        self.curveMaker.TubeRadius = 1.5
        self.curveMaker.ModelColor = [0.678, 0.847, 0.902]   

    def getParameterNode(self):
        return SmartNeedleParameterNode(super().getParameterNode())
    
    def initializeConnectorDefaults(self, connectorNode) -> None:
        if connectorNode is None:
            return
        # Only initialize once, so existing connectors keep their settings
        if connectorNode.GetAttribute("SmartNeedle.Initialized") == "True":
            return
        # Default client mode
        connectorNode.SetTypeClient("localhost", 18944)
        if not connectorNode.GetName() or connectorNode.GetName().startswith("vtkMRMLIGTLConnectorNode"):
            connectorNode.SetName("NeedleShapeConnector")
        connectorNode.SetAttribute("SmartNeedle.Initialized", "True")
        logging.info(f"Initialized connector '{connectorNode.GetName()}' with default OpenIGTLink settings")

    
      # Extract information from NeedleShapeHeader STRING message
    def getCurrentHeader(self):
        headerText = self.needleShapeHeaderNode.GetText()
        if not headerText:
            return (None, None, None, None)
        parts = headerText.split(';')
        if len(parts) < 4:
            return (None, None, None, None)
        try:
            timestamp = parts[0]
            package_number = int(parts[1])
            num_points = int(parts[2])
            frame_id = parts[3]
        except (ValueError, IndexError):
            return (None, None, None, None)
        return (timestamp, package_number, num_points, frame_id)
    
    # Return coordinates of the last point in array (needle tip)
    def getCurrentTipCoordinates(self):
        if self.needleShapePointsNode is None:
            print("No markups node")
            return None
        n = self.needleShapePointsNode.GetNumberOfControlPoints()
        if n < 1:
            return None
        p = [0.0, 0.0, 0.0]
        self.needleShapePointsNode.GetNthControlPointPositionWorld(n-1, p)
        return tuple(p)

    def startShapeUpdate(self, connectorNode) -> None:
        if not connectorNode:
            raise ValueError("OpenIGTLink connector node is invalid")
        self.activeConnectorNode = connectorNode
        self.initializeConnectorDefaults(self.activeConnectorNode)
        self.activeConnectorNode.Stop()
        self.activeConnectorNode.RegisterIncomingMRMLNode(self.needleShapeHeaderNode)
        self.activeConnectorNode.RegisterIncomingMRMLNode(self.needleShapePointsNode)
        self.activeConnectorNode.Start()
        self.curveMaker.AutomaticUpdate = True

    def stopShapeUpdate(self) -> None:
        if self.activeConnectorNode:
            self.activeConnectorNode.Stop() # Deactivate connection
        self.activeConnectorNode = None

    def getNeedleShape(self):
        # Update needle model
        if self.needleShapePointsNode is not None:
            if self.needleShapePointsNode.GetNumberOfControlPoints() >= 2:
                self.curveMaker.updateCurve()
        timestamp, package_number, num_points, frame_id, tip_coordinates = (None, None, None, None, None)
        # Header
        if self.needleShapeHeaderNode is not None:
            headerText = self.needleShapeHeaderNode.GetText()
            if headerText:
                parts = headerText.split(';')
                if len(parts) >= 4:
                    try:
                        timestamp = parts[0]
                        package_number = int(parts[1])
                        num_points = int(parts[2])
                        frame_id = parts[3]
                    except (ValueError, IndexError):
                        pass
        # Tip
        if self.needleShapePointsNode is not None:
            n = self.needleShapePointsNode.GetNumberOfControlPoints()
            if n >= 1:
                p = [0.0, 0.0, 0.0]
                self.needleShapePointsNode.GetNthControlPointPositionWorld(n - 1, p)
                tip_coordinates = tuple(p)
        return {
            "timestamp": timestamp,
            "package_number": package_number,
            "num_points": num_points,
            "frame_id": frame_id,
            "tip_coordinates": tip_coordinates,
        }
    
    def copyShape(self, name):
        if not name:
            raise ValueError("Shape name is empty.")
        if self.needleShapePointsNode is None:
            raise ValueError("Needle shape points node does not exist.")
        if self.needleShapeHeaderNode is None:
            raise ValueError("Needle shape header node does not exist.")
        if self.needleModelNode is None:
            raise ValueError("Needle shape model node does not exist.")
        # Copy needle shape points
        copyShapePoints = slicer.mrmlScene.AddNewNodeByClass('vtkMRMLMarkupsFiducialNode', name+'_NeedleShape')
        copyShapePoints.CopyContent(self.needleShapePointsNode)
        # Copy needle shape header
        copyShapeHeader = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLTextNode", f"{name}_NeedleShapeHeader")
        copyShapeHeader.CopyContent(self.needleShapeHeaderNode)
        # Copy needle shape model
        copyShapeModel = slicer.mrmlScene.AddNewNodeByClass('vtkMRMLModelNode', name+'_NeedleModel')
        copyShapeModel.CopyContent(self.needleModelNode)
        return copyShapePoints, copyShapeModel, copyShapeHeader
#
# SmartNeedleTest
#

class SmartNeedleTest(ScriptedLoadableModuleTest):
    """
    This is the test case for your scripted module.
    Uses ScriptedLoadableModuleTest base class, available at:
    https://github.com/Slicer/Slicer/blob/main/Base/Python/slicer/ScriptedLoadableModule.py
    """

    def setUp(self):
        """Do whatever is needed to reset the state - typically a scene clear will be enough."""
        slicer.mrmlScene.Clear()

    def runTest(self):
        """Run as few or as many tests as needed here."""
        self.delayDisplay("Starting SmartNeedle tests")
        self.setUp()
        self.test_internalNodesCreated()
        self.test_getCurrentHeader()
        self.test_getCurrentTipCoordinates()
        self.test_getNeedleShape()
        self.test_copyShape()
        self.delayDisplay("SmartNeedle tests completed")

    def test_internalNodesCreated(self):
        logic = SmartNeedleLogic()

        self.assertIsNotNone(logic.needleShapePointsNode)
        self.assertIsNotNone(logic.needleShapeHeaderNode)
        self.assertIsNotNone(logic.needleModelNode)

        self.assertEqual(logic.needleShapePointsNode.GetClassName(), "vtkMRMLMarkupsFiducialNode")
        self.assertEqual(logic.needleShapeHeaderNode.GetClassName(), "vtkMRMLTextNode")
        self.assertEqual(logic.needleModelNode.GetClassName(), "vtkMRMLModelNode")

    def test_getCurrentHeader(self):
        logic = SmartNeedleLogic()

        logic.needleShapeHeaderNode.SetText("2026-03-25 12:34:56.000;42;8;Tracker")
        timestamp, package_number, num_points, frame_id = logic.getCurrentHeader()

        self.assertEqual(timestamp, "2026-03-25 12:34:56.000")
        self.assertEqual(package_number, 42)
        self.assertEqual(num_points, 8)
        self.assertEqual(frame_id, "Tracker")

        logic.needleShapeHeaderNode.SetText("")
        timestamp, package_number, num_points, frame_id = logic.getCurrentHeader()
        self.assertIsNone(timestamp)
        self.assertIsNone(package_number)
        self.assertIsNone(num_points)
        self.assertIsNone(frame_id)

    def test_getCurrentTipCoordinates(self):
        logic = SmartNeedleLogic()
        logic.needleShapePointsNode.RemoveAllControlPoints()

        self.assertIsNone(logic.getCurrentTipCoordinates())

        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(1.0, 2.0, 3.0))
        tip = logic.getCurrentTipCoordinates()
        self.assertEqual(tip, (1.0, 2.0, 3.0))

        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(4.0, 5.0, 6.0))
        tip = logic.getCurrentTipCoordinates()
        self.assertEqual(tip, (4.0, 5.0, 6.0))

    def test_getNeedleShape(self):
        logic = SmartNeedleLogic()
        logic.needleShapePointsNode.RemoveAllControlPoints()
        logic.needleShapeHeaderNode.SetText("2026-03-25 12:34:56.000;7;2;Tracker")
        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(10.0, 20.0, 30.0))
        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(40.0, 50.0, 60.0))

        shape = logic.getNeedleShape()

        self.assertEqual(shape["timestamp"], "2026-03-25 12:34:56.000")
        self.assertEqual(shape["package_number"], 7)
        self.assertEqual(shape["num_points"], 2)
        self.assertEqual(shape["frame_id"], "Tracker")
        self.assertEqual(shape["tip_coordinates"], (40.0, 50.0, 60.0))

    def test_copyShape(self):
        logic = SmartNeedleLogic()
        logic.needleShapePointsNode.RemoveAllControlPoints()
        logic.needleShapeHeaderNode.SetText("2026-03-25 12:34:56.000;5;2;Tracker")
        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(1.0, 2.0, 3.0))
        logic.needleShapePointsNode.AddControlPointWorld(vtk.vtkVector3d(4.0, 5.0, 6.0))

        copyPoints, copyModel, copyHeader = logic.copyShape("TestShape")

        self.assertEqual(copyPoints.GetName(), "TestShape_NeedleShape")
        self.assertEqual(copyModel.GetName(), "TestShape_NeedleModel")
        self.assertEqual(copyHeader.GetName(), "TestShape_NeedleShapeHeader")
        self.assertEqual(copyPoints.GetNumberOfControlPoints(), 2)
        self.assertEqual(copyHeader.GetText(), "2026-03-25 12:34:56.000;5;2;Tracker")
