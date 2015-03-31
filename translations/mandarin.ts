<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en_US">
<context>
    <name>AboutWindow</name>
    <message>
        <location filename="../qml/AboutWindow.qml" line="15"/>
        <source>About</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/AboutWindow.qml" line="34"/>
        <source>Version: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/AboutWindow.qml" line="40"/>
        <source>Copyright 2013 Philip Schuchardt</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>AllCavesTabWidget</name>
    <message>
        <source>Overview</source>
        <translation type="obsolete">概览</translation>
    </message>
    <message>
        <source>This is the overview of all the caves</source>
        <translation type="obsolete">洞穴概览</translation>
    </message>
    <message>
        <source>Connections</source>
        <translation type="obsolete">连接</translation>
    </message>
    <message>
        <source>This is the Connection page</source>
        <translation type="obsolete">连接页面</translation>
    </message>
</context>
<context>
    <name>BackSightCalibrationEditor</name>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="20"/>
        <source>&lt;b&gt;Back Sights&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;后视&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="43"/>
        <source>Compass calibration</source>
        <translation type="unfinished">方位校准</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="61"/>
        <source>&lt;p&gt;Back sight compass calibration allows you to correct an instrument that&apos;s off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.&lt;/p&gt;
&lt;b&gt;UncorrectedValue + Calibration = TrueValue&lt;/b&gt;
&lt;p&gt; For example, the reading in the cave was 180°. Your instrument is off by -2°. The bearing is really
182° instead of 180 (because your insturment is subtracting 2° at every shot).  So you need to enter 2
for the calibration to correct it. UncorrectedValue = 180°,
Calibration = 2°, so 180° + (2°) = 182° &lt;/p&gt;</source>
        <translation type="unfinished">&lt;p&gt;你可以通过后视方位校准来修正仪器偏差
将校准值添加到未修正的读数上
(你读到的仪器度数) 找到修正值.&lt;/p&gt;
&lt;b&gt;未修正值 + 校准值 = 修正值&lt;/b&gt;
&lt;p&gt; 例如, 你在洞内的读数是180°. 校准发现你的仪器偏差 -2°. 这时真正的方位是
182°而非 180 (因为你的仪器在每测杆少了 2° ).  因此，你要加2来
修正仪器读数。未修正值 = 180°,
校准值 = 2°, 因此 180° + (2°) = 182° &lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="76"/>
        <source>Clino calibration</source>
        <translation type="unfinished">倾角校准</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="94"/>
        <source>&lt;p&gt;Back sight clino calibration allows you to correct an instrument that&apos;s off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.&lt;/p&gt;
&lt;b&gt;UncorrectedValue + Calibration = TrueValue&lt;/b&gt;
&lt;p&gt; For example, the reading in the cave was +4°. Your instrument is off by +1°. The bearing is really
+3° instead of +4° (because your insturment is adding extra 1° at every shot).  So you need to enter -1
for the calibration to correct it. UncorrectedValue = +4°,
Calibration = -1°, so +4° + (-1°) = +3° &lt;/p&gt;</source>
        <translation type="unfinished">&lt;p&gt;你可以通过后视倾角校准来修正仪器偏差
将校准值添加到未修正的读数上
(你读到的仪器度数) 找到修正值.&lt;/p&gt;
&lt;b&gt;未修正值 + 校准值 = 修正值&lt;/b&gt;
&lt;p&gt;  例如, 你在洞内的读数是 +4°. 校准发现你的仪器偏差  +1°。 这时真正的角度是
+3° 而非 +4° (因为你的仪器在每测杆多了 1° ).  因此，你要减1来
修正仪器读数。未修正值 = +4°,
校准值 = -1°, 因此 +4° + (-1°) = +3° &lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="116"/>
        <source>Corrected &lt;i&gt;Compass&lt;/i&gt;</source>
        <translation type="unfinished">修正 &lt;i&gt;方位&lt;/i&gt;</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="131"/>
        <source>Corrected compass allow you to entry back sights as if they were read as
            a front sight.  This will &lt;b&gt;subtract 180°&lt;/b&gt; to all back sight compass readings to get
            the true value.</source>
        <translation type="unfinished">你可以输入后视修正方位，就如你
            前视一样。  你要 &lt;b&gt;减去 180°&lt;/b&gt; 从而得到
            修正值.</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="146"/>
        <source>Corrected &lt;i&gt;Clino&lt;/i&gt;</source>
        <translation type="unfinished">修正 &lt;i&gt;倾角&lt;/i&gt;</translation>
    </message>
    <message>
        <location filename="../qml/BackSightCalibrationEditor.qml" line="160"/>
        <source>Corrected clino allow you to entry back sights as if they were read as
            a front sight.  This will &lt;b&gt;subtract 90°&lt;/b&gt; to all back sight clino readings to get
            the true value.</source>
        <translation type="unfinished">你可以输入后视修正倾角，就如你
            前视一样。  你要 &lt;b&gt;减去 90°&lt;/b&gt; 从而得到
            修正值.</translation>
    </message>
</context>
<context>
    <name>BackSightReadingBox</name>
    <message>
        <location filename="../qml/BackSightReadingBox.qml" line="13"/>
        <source>bs</source>
        <translation type="unfinished">后视</translation>
    </message>
</context>
<context>
    <name>CalibrationEditor</name>
    <message>
        <location filename="../qml/CalibrationEditor.qml" line="33"/>
        <source>Calibration</source>
        <translation type="unfinished">校准</translation>
    </message>
    <message>
        <location filename="../qml/CalibrationEditor.qml" line="87"/>
        <source>Hmm, you need to &lt;b&gt;check&lt;/b&gt; either &lt;i&gt;front&lt;/i&gt; or &lt;i&gt;back sights&lt;/i&gt; box, or both, depending on your data.</source>
        <translation type="unfinished">嗯，依据你的数据，你需要 &lt;b&gt;检查&lt;/b&gt; 或者 &lt;i&gt;前视&lt;/i&gt; 或者 &lt;i&gt;后视&lt;/i&gt; 读数, 或者两个都检查。</translation>
    </message>
</context>
<context>
    <name>CameraAzimuthSettings</name>
    <message>
        <location filename="../qml/CameraAzimuthSettings.qml" line="25"/>
        <source>North</source>
        <translation type="unfinished">北</translation>
    </message>
    <message>
        <location filename="../qml/CameraAzimuthSettings.qml" line="36"/>
        <source>West</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraAzimuthSettings.qml" line="63"/>
        <source>°</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraAzimuthSettings.qml" line="69"/>
        <source>East</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraAzimuthSettings.qml" line="80"/>
        <source>South</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CameraOptionsTab</name>
    <message>
        <location filename="../qml/CameraOptionsTab.qml" line="29"/>
        <source>Azimuth</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraOptionsTab.qml" line="43"/>
        <source>Vertical Angle</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraOptionsTab.qml" line="58"/>
        <source>Projection</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CameraProjectionSettings</name>
    <message>
        <location filename="../qml/CameraProjectionSettings.qml" line="24"/>
        <source>Field of View</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraProjectionSettings.qml" line="38"/>
        <source>°</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CameraVerticalAngleSettings</name>
    <message>
        <location filename="../qml/CameraVerticalAngleSettings.qml" line="27"/>
        <source>°</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraVerticalAngleSettings.qml" line="37"/>
        <source>Plan</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CameraVerticalAngleSettings.qml" line="48"/>
        <source>Profile</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CaveDataSidebarPage</name>
    <message>
        <location filename="qml/CaveDataSidebarPage.qml" line="75"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="../qml/CaveDataSidebarPage.qml" line="88"/>
        <source>Only 1 Cave</source>
        <translation type="unfinished">单个洞穴</translation>
    </message>
    <message>
        <location filename="../qml/CaveDataSidebarPage.qml" line="95"/>
        <source>Add Cave</source>
        <translation type="unfinished">添加洞穴</translation>
    </message>
    <message>
        <location filename="../qml/CaveDataSidebarPage.qml" line="116"/>
        <source>Add Trip</source>
        <translation type="unfinished">添加航程</translation>
    </message>
</context>
<context>
    <name>CaveLeadPage</name>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="24"/>
        <source>Filter...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="35"/>
        <source>Lead Distance from:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="39"/>
        <source>No Station</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="52"/>
        <source>Lead distance from a station, calculates the &lt;b&gt;line of sight&lt;/b&gt; distance from the station to all the leads.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="88"/>
        <source>Distance to </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="94"/>
        <source>leadCompleted</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLeadPage.qml" line="164"/>
        <source>Goto</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CaveLengthAndDepth</name>
    <message>
        <location filename="../qml/CaveLengthAndDepth.qml" line="16"/>
        <source>Length:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CaveLengthAndDepth.qml" line="23"/>
        <source>Depth:</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CavePage</name>
    <message>
        <location filename="../qml/CavePage.qml" line="22"/>
        <source>Trip=</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CavePage.qml" line="101"/>
        <source>Leads</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/CavePage.qml" line="133"/>
        <source>Add Trip</source>
        <translation type="unfinished">添加航程</translation>
    </message>
</context>
<context>
    <name>CaveTabWidget</name>
    <message>
        <location filename="../qml/CaveTabWidget.qml" line="16"/>
        <source>Overview</source>
        <translation type="unfinished">概览</translation>
    </message>
    <message>
        <source>Notes</source>
        <translation type="obsolete">备注</translation>
    </message>
    <message>
        <source>This is the Notes page</source>
        <translation type="obsolete">备注页面</translation>
    </message>
    <message>
        <source>Team</source>
        <translation type="obsolete">团队</translation>
    </message>
    <message>
        <source>This is the Team page</source>
        <translation type="obsolete">团队页面</translation>
    </message>
    <message>
        <source>Pictures</source>
        <translation type="obsolete">图片</translation>
    </message>
    <message>
        <source>This is the Pictures page</source>
        <translation type="obsolete">图片页面</translation>
    </message>
    <message>
        <source>Log</source>
        <translation type="obsolete">日志</translation>
    </message>
    <message>
        <source>This is the Log page</source>
        <translation type="obsolete">日志页面</translation>
    </message>
</context>
<context>
    <name>CaveTreeDelegate</name>
    <message>
        <source>Remove</source>
        <translation type="obsolete">移动</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="obsolete">取消</translation>
    </message>
    <message>
        <location filename="../qml/CaveTreeDelegate.qml" line="126"/>
        <source>Trips</source>
        <translation type="unfinished">航程</translation>
    </message>
</context>
<context>
    <name>CavewhereMainWindow</name>
    <message>
        <location filename="../qml/CavewhereMainWindow.qml" line="72"/>
        <source>Loading</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DataBox</name>
    <message>
        <location filename="../qml/DataBox.qml" line="77"/>
        <source>Remove Chunk</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DataMainPage</name>
    <message>
        <location filename="../qml/DataMainPage.qml" line="35"/>
        <source>All Caves</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DataRightClickMouseMenu</name>
    <message>
        <location filename="../qml/DataRightClickMouseMenu.qml" line="20"/>
        <source>Remove </source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>DataSideBar</name>
    <message>
        <location filename="../qml/DataSideBar.qml" line="35"/>
        <source>Caves</source>
        <translation type="unfinished">洞穴</translation>
    </message>
    <message>
        <source>Surface</source>
        <translation type="obsolete">地表</translation>
    </message>
    <message>
        <source>This is the Surface page</source>
        <translation type="obsolete">地表页面</translation>
    </message>
</context>
<context>
    <name>DeclainationEditor</name>
    <message>
        <location filename="../qml/DeclainationEditor.qml" line="18"/>
        <location filename="../qml/DeclainationEditor.qml" line="30"/>
        <source>Declination</source>
        <translation type="unfinished">磁偏角</translation>
    </message>
    <message>
        <location filename="../qml/DeclainationEditor.qml" line="46"/>
        <source>&lt;p&gt;Magnetic declination is the &lt;b&gt;angle between magnetic north and true north&lt;/b&gt;&lt;/p&gt;
            Cavewhere calculates the true bearing (&lt;b&gt;TB&lt;/b&gt;) by adding declination (&lt;b&gt;D&lt;/b&gt;) to magnetic bearing (&lt;b&gt;MB&lt;/b&gt;).
            &lt;center&gt;&lt;b&gt;MB + D = TB&lt;/b&gt;&lt;/center&gt;</source>
        <translation type="unfinished">&lt;p&gt;磁偏角是 &lt;b&gt;真北和磁北之间的角度差&lt;/b&gt;&lt;/p&gt;
            Cavewhere 通过  将磁偏角 (&lt;b&gt;D&lt;/b&gt;) 与磁北方位角 (&lt;b&gt;MB&lt;/b&gt;).相加得到真北方位角(&lt;b&gt;TB&lt;/b&gt;)
            &lt;center&gt;&lt;b&gt;MB + D = TB&lt;/b&gt;&lt;/center&gt;</translation>
    </message>
</context>
<context>
    <name>DrawLengthInteraction</name>
    <message>
        <location filename="../qml/DrawLengthInteraction.qml" line="133"/>
        <source>Done</source>
        <translation type="unfinished">完成</translation>
    </message>
    <message>
        <location filename="../qml/DrawLengthInteraction.qml" line="142"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the length&apos;s first point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 长度第一点</translation>
    </message>
    <message>
        <location filename="../qml/DrawLengthInteraction.qml" line="177"/>
        <source>&lt;b&gt; Click &lt;/b&gt; the length&apos;s second point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 长度第二点</translation>
    </message>
</context>
<context>
    <name>ExportImportButtons</name>
    <message>
        <location filename="../qml/ExportImportButtons.qml" line="118"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportImportButtons.qml" line="131"/>
        <source>Current trip</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportImportButtons.qml" line="140"/>
        <location filename="../qml/ExportImportButtons.qml" line="161"/>
        <source>Current cave</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportImportButtons.qml" line="149"/>
        <source>Region (all caves)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportImportButtons.qml" line="175"/>
        <source>Import</source>
        <translation type="unfinished">输入</translation>
    </message>
</context>
<context>
    <name>ExportSurveyMenuItem</name>
    <message>
        <location filename="../qml/ExportSurveyMenuItem.qml" line="19"/>
        <source> - </source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ExportViewTab</name>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="79"/>
        <source>Remove</source>
        <translation type="unfinished">移动</translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="115"/>
        <source>File type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="124"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="137"/>
        <source>Options</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="165"/>
        <source>Paper Size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="205"/>
        <source>Width</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="219"/>
        <location filename="../qml/ExportViewTab.qml" line="237"/>
        <source>in</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="224"/>
        <source>Height</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="244"/>
        <source>Resolution</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="264"/>
        <source>DPI</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="283"/>
        <source>Orientation</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="288"/>
        <source>Portrait</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="300"/>
        <source>Landscrape</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="307"/>
        <source>Layers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="367"/>
        <source>Layer Groups</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="420"/>
        <source>Size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="429"/>
        <source>x</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="441"/>
        <source>Position</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="445"/>
        <source>x:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="454"/>
        <source>y:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="465"/>
        <source>Rotation</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="483"/>
        <source>Properies of </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="543"/>
        <source>Export to </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="555"/>
        <source>Letter</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="566"/>
        <source>Legal</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="577"/>
        <source>A4</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ExportViewTab.qml" line="588"/>
        <source>Custom Size</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>FileButtonAndMenu</name>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="24"/>
        <source>File</source>
        <translation type="unfinished">文件</translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="27"/>
        <source>New</source>
        <translation type="unfinished">新建</translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="38"/>
        <source>Open</source>
        <translation type="unfinished">打开</translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="49"/>
        <source>Save</source>
        <translation type="unfinished">存入</translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="62"/>
        <source>Save As</source>
        <translation type="unfinished">另存为</translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="72"/>
        <source>About Cavewhere</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/FileButtonAndMenu.qml" line="77"/>
        <source>Quit</source>
        <translation type="unfinished">停止</translation>
    </message>
</context>
<context>
    <name>FrontSightCalibrationEditor</name>
    <message>
        <location filename="../qml/FrontSightCalibrationEditor.qml" line="20"/>
        <source>&lt;b&gt;Front Sights&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;前视&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../qml/FrontSightCalibrationEditor.qml" line="41"/>
        <source>Compass calibration</source>
        <translation type="unfinished">方位校准</translation>
    </message>
    <message>
        <location filename="../qml/FrontSightCalibrationEditor.qml" line="59"/>
        <source>&lt;p&gt;Front sight compass calibration allows you to correct an instrument that&apos;s off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.&lt;/p&gt;
&lt;b&gt;UncorrectedValue + Calibration = TrueValue&lt;/b&gt;
&lt;p&gt; For example, the reading in the cave was 180°. Your instrument is off by -2°. The bearing is really
182° instead of 180 (because your insturment is subtracting 2° at every shot).  So you need to enter 2
for the calibration to correct it. UncorrectedValue = 180°,
Calibration = 2°, so 180° + (2°) = 182° &lt;/p&gt;</source>
        <translation type="unfinished">&lt;p&gt;你可以通过前视方位校准来修正仪器偏差
将校准值添加到未修正的读数上
(你读到的仪器度数) 找到修正值.&lt;/p&gt;
&lt;b&gt;未修正值 + 校准值 = 修正值&lt;/b&gt;
&lt;p&gt; 例如, 你在洞内的读数是180°. 校准发现你的仪器偏差 -2°. 这时真正的方位是
182°而非 180 (因为你的仪器在每测杆少了 2° ).  因此，你要加2来
修正仪器读数。未修正值 = 180°,
校准值 = 2°, 因此 180° + (2°) = 182° &lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../qml/FrontSightCalibrationEditor.qml" line="74"/>
        <source>Clino calibration</source>
        <translation type="unfinished">倾角校准</translation>
    </message>
    <message>
        <location filename="../qml/FrontSightCalibrationEditor.qml" line="94"/>
        <source>&lt;p&gt;Front sight clino calibration allows you to correct an instrument that&apos;s off.
The calibration is added to uncorrected value
(the value you read off the instrument) to find the true value.&lt;/p&gt;
&lt;b&gt;UncorrectedValue + Calibration = TrueValue&lt;/b&gt;
&lt;p&gt; For example, the reading in the cave was +4°. Your instrument is off by +1°. The bearing is really
+3° instead of +4° (because your insturment is adding extra 1° at every shot).  So you need to enter -1
for the calibration to correct it. UncorrectedValue = +4°,
Calibration = -1°, so +4° + (-1°) = +3° &lt;/p&gt;</source>
        <translation type="unfinished">&lt;p&gt;你可以通过前视倾角校准来修正仪器偏差
将校准值添加到未修正的读数上
(你读到的仪器度数) 找到修正值.&lt;/p&gt;
&lt;b&gt;未修正值 + 校准值 = 修正值&lt;/b&gt;
&lt;p&gt;  例如, 你在洞内的读数是 +4°. 校准发现你的仪器偏差  +1°。 这时真正的角度是
+3° 而非 +4° (因为你的仪器在每测杆多了 1° ).  因此，你要减1来
修正仪器读数。未修正值 = +4°,
校准值 = -1°, 因此 +4° + (-1°) = +3° &lt;/p&gt;</translation>
    </message>
</context>
<context>
    <name>FrontSightReadingBox</name>
    <message>
        <location filename="../qml/FrontSightReadingBox.qml" line="12"/>
        <source>fs</source>
        <translation type="unfinished">前视</translation>
    </message>
</context>
<context>
    <name>GlobalShotStdevWidget</name>
    <message>
        <location filename="../qml/GlobalShotStdevWidget.qml" line="22"/>
        <source>Distance Std:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/GlobalShotStdevWidget.qml" line="35"/>
        <source>Compass Std:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/GlobalShotStdevWidget.qml" line="48"/>
        <source>Clino Std:</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>HelpArea</name>
    <message>
        <location filename="../qml/HelpArea.qml" line="58"/>
        <source>No documentation</source>
        <translation type="unfinished">暂无文本</translation>
    </message>
</context>
<context>
    <name>LeadInfoEditor</name>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="11"/>
        <source>Lead Info</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="50"/>
        <source>Passage Type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="56"/>
        <source>Passage Fill</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="63"/>
        <source>Passage Traversal</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="71"/>
        <source>Stream Type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="78"/>
        <source>Air</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadInfoEditor.qml" line="85"/>
        <source>Gear Needed</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LeadPoint</name>
    <message>
        <location filename="../qml/LeadPoint.qml" line="60"/>
        <source>Completed</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LeadPoint.qml" line="83"/>
        <source>Notes</source>
        <translation type="unfinished">备注</translation>
    </message>
</context>
<context>
    <name>LengthInput</name>
    <message>
        <location filename="qml/LengthInput.qml" line="13"/>
        <source></source>
        <translation></translation>
    </message>
</context>
<context>
    <name>LicenseWindow</name>
    <message>
        <location filename="../qml/LicenseWindow.qml" line="12"/>
        <source>License Agreement</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LicenseWindow.qml" line="40"/>
        <source>Close Cavewhere</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LicenseWindow.qml" line="48"/>
        <source>Accept</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LoadNotesIconButton</name>
    <message>
        <location filename="../qml/LoadNotesIconButton.qml" line="19"/>
        <source>Load</source>
        <translation type="unfinished">载入</translation>
    </message>
    <message>
        <location filename="../qml/LoadNotesIconButton.qml" line="27"/>
        <source>Images (*.png *.jpg *.jpeg *.jp2 *.tiff)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LoadNotesIconButton.qml" line="28"/>
        <source>Load Images</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LoadNotesWidget</name>
    <message>
        <location filename="../qml/LoadNotesWidget.qml" line="59"/>
        <source>No notes found...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/LoadNotesWidget.qml" line="70"/>
        <source>Add scanned notes</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>MainSideBar</name>
    <message>
        <location filename="../qml/MainSideBar.qml" line="109"/>
        <source>View</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/MainSideBar.qml" line="117"/>
        <source>Data</source>
        <translation type="unfinished">数据</translation>
    </message>
</context>
<context>
    <name>NoteDPIInteraction</name>
    <message>
        <location filename="../qml/NoteDPIInteraction.qml" line="16"/>
        <source>&lt;b&gt;Length on the paper&lt;/b&gt;</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NoteLeadInteraction</name>
    <message>
        <location filename="../qml/NoteLeadInteraction.qml" line="16"/>
        <source>Click to add a lead</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NoteNorthInteraction</name>
    <message>
        <location filename="../qml/NoteNorthInteraction.qml" line="122"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the north arrow&apos;s first point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 指北针第一点</translation>
    </message>
    <message>
        <location filename="../qml/NoteNorthInteraction.qml" line="154"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the north arrow&apos;s second point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 指北针第二点</translation>
    </message>
</context>
<context>
    <name>NoteNorthUpInput</name>
    <message>
        <location filename="../qml/NoteNorthUpInput.qml" line="37"/>
        <source>North</source>
        <translation type="unfinished">北</translation>
    </message>
    <message>
        <location filename="qml/NoteNorthUpInput.qml" line="37"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="../qml/NoteNorthUpInput.qml" line="52"/>
        <source>&amp;deg</source>
        <translation type="unfinished">&amp;度数</translation>
    </message>
</context>
<context>
    <name>NoteResolution</name>
    <message>
        <location filename="../qml/NoteResolution.qml" line="26"/>
        <source>Image Info</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/NoteResolution.qml" line="39"/>
        <source>Image Resolution</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/NoteResolution.qml" line="58"/>
        <source>&lt;b&gt;Propagate resolution&lt;/b&gt; for each note in </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/NoteResolution.qml" line="64"/>
        <source>&lt;b&gt;Reset&lt;/b&gt; to original</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>NoteScaleInput</name>
    <message>
        <source>Scale</source>
        <translation type="obsolete">比例尺</translation>
    </message>
    <message>
        <source>On Paper</source>
        <translation type="obsolete">纸上</translation>
    </message>
    <message>
        <source>=</source>
        <translation type="obsolete">=</translation>
    </message>
    <message>
        <source>In Cave</source>
        <translation type="obsolete">洞内</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="113"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <source>Weird scaling units</source>
        <translation type="obsolete">比例尺单位</translation>
    </message>
    <message>
        <source>1:</source>
        <translation type="obsolete">1:</translation>
    </message>
</context>
<context>
    <name>NoteScaleInteraction</name>
    <message>
        <location filename="../qml/NoteScaleInteraction.qml" line="17"/>
        <source>&lt;b&gt;In cave length&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;洞内长度&lt;/b&gt;</translation>
    </message>
    <message>
        <source>Done</source>
        <translation type="obsolete">完成</translation>
    </message>
    <message>
        <source>&lt;b&gt;Click&lt;/b&gt; the length&apos;s first point</source>
        <translation type="obsolete">&lt;b&gt;点击&lt;/b&gt; 长度第一点</translation>
    </message>
    <message>
        <source>&lt;b&gt; Click &lt;/b&gt; the length&apos;s second point</source>
        <translation type="obsolete">&lt;b&gt;点击&lt;/b&gt; 长度第二点</translation>
    </message>
</context>
<context>
    <name>NoteStationInteraction</name>
    <message>
        <location filename="../qml/NoteStationInteraction.qml" line="24"/>
        <source>Click to add new station</source>
        <translation type="unfinished">点击添加测点</translation>
    </message>
</context>
<context>
    <name>NoteTransformEditor</name>
    <message>
        <location filename="../qml/NoteTransformEditor.qml" line="50"/>
        <source>Scrap Info</source>
        <translation type="unfinished">小块信息</translation>
    </message>
    <message>
        <location filename="../qml/NoteTransformEditor.qml" line="86"/>
        <source>Auto Calculate</source>
        <translation type="unfinished">自动计算</translation>
    </message>
    <message>
        <location filename="../qml/NoteTransformEditor.qml" line="113"/>
        <source>You can set the direction of &lt;b&gt;north&lt;/b&gt; relative to page for a scrap.
            Cavewhere only uses &lt;b&gt;north&lt;/b&gt; to help you automatically label stations.</source>
        <translation type="unfinished">你可相对纸张设置小块指 &lt;b&gt;北&lt;/b&gt; 向。
            Cavewhere 只用 &lt;b&gt;北&lt;/b&gt; 帮助你自动标注测点.</translation>
    </message>
    <message>
        <location filename="../qml/NoteTransformEditor.qml" line="128"/>
        <source>You can set the &lt;b&gt;scale&lt;/b&gt; of the scrap.</source>
        <translation type="unfinished">你可设置小块&lt;b&gt;比例尺&lt;/b&gt;.</translation>
    </message>
</context>
<context>
    <name>NotesGallery</name>
    <message>
        <location filename="../qml/NotesGallery.qml" line="328"/>
        <source>Rotate</source>
        <translation type="unfinished">旋转</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="342"/>
        <source>Carpet</source>
        <translation type="unfinished">覆盖</translation>
    </message>
    <message>
        <source>Load</source>
        <translation type="obsolete">载入</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="220"/>
        <source>Okay</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="379"/>
        <source>Back</source>
        <translation type="unfinished">返回</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="392"/>
        <source>Select</source>
        <translation type="unfinished">选择</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="401"/>
        <source>Add</source>
        <translation type="unfinished">添加</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="416"/>
        <source>Station</source>
        <translation type="unfinished">测点</translation>
    </message>
    <message>
        <location filename="../qml/NotesGallery.qml" line="425"/>
        <source>Lead</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Scrap</source>
        <translation type="obsolete">舍弃</translation>
    </message>
</context>
<context>
    <name>PaperMarginGroupBox</name>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="14"/>
        <source>Margins - inches</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="54"/>
        <source>Top</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="62"/>
        <source>Left</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="68"/>
        <source>All</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="81"/>
        <source>Right</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperMarginGroupBox.qml" line="89"/>
        <source>Bottom</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PaperMarginSpinBox</name>
    <message>
        <location filename="../qml/PaperMarginSpinBox.qml" line="21"/>
        <source>Top</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PaperScaleInput</name>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="45"/>
        <source>Scale</source>
        <translation type="unfinished">比例尺</translation>
    </message>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="51"/>
        <source>On Paper</source>
        <translation type="unfinished">纸上</translation>
    </message>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="64"/>
        <location filename="../qml/PaperScaleInput.qml" line="84"/>
        <source>=</source>
        <translation type="unfinished">=</translation>
    </message>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="69"/>
        <source>In Cave</source>
        <translation type="unfinished">洞内</translation>
    </message>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="96"/>
        <source>red</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/PaperScaleInput.qml" line="99"/>
        <source>Weird scaling units</source>
        <translation type="unfinished">比例尺单位</translation>
    </message>
</context>
<context>
    <name>ProjectionSlider</name>
    <message>
        <location filename="../qml/ProjectionSlider.qml" line="31"/>
        <source>Orthognal</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/ProjectionSlider.qml" line="53"/>
        <source>Perspective</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>RemoveAskBox</name>
    <message>
        <location filename="../qml/RemoveAskBox.qml" line="73"/>
        <source>Remove</source>
        <translation type="unfinished">移动</translation>
    </message>
    <message>
        <location filename="../qml/RemoveAskBox.qml" line="81"/>
        <source>Cancel</source>
        <translation type="unfinished">取消</translation>
    </message>
</context>
<context>
    <name>RenderingSideBar</name>
    <message>
        <location filename="../qml/RenderingSideBar.qml" line="53"/>
        <source>View</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/RenderingSideBar.qml" line="54"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ScaleInput</name>
    <message>
        <location filename="../qml/ScaleInput.qml" line="33"/>
        <source>Scale</source>
        <translation type="unfinished">比例尺</translation>
    </message>
    <message>
        <location filename="../qml/ScaleInput.qml" line="52"/>
        <source>On Paper</source>
        <translation type="unfinished">纸上</translation>
    </message>
    <message>
        <location filename="../qml/ScaleInput.qml" line="68"/>
        <location filename="../qml/ScaleInput.qml" line="104"/>
        <source>=</source>
        <translation type="unfinished">=</translation>
    </message>
    <message>
        <location filename="../qml/ScaleInput.qml" line="86"/>
        <source>In Cave</source>
        <translation type="unfinished">洞内</translation>
    </message>
    <message>
        <location filename="../qml/ScaleInput.qml" line="111"/>
        <source>1:</source>
        <translation type="unfinished">1:</translation>
    </message>
    <message>
        <location filename="../qml/ScaleInput.qml" line="119"/>
        <source>Weird scaling units</source>
        <translation type="unfinished">比例尺单位</translation>
    </message>
</context>
<context>
    <name>ScrapInteraction</name>
    <message>
        <source>Trace a scrap by &lt;b&gt;clicking&lt;/b&gt; points around it &lt;br&gt;
        Press &lt;b&gt;spacebar&lt;/b&gt; to add a new scrap</source>
        <translation type="obsolete">通过 &lt;b&gt;点击&lt;/b&gt; 碎片周围的点 &lt;br&gt;追踪碎片
        按 &lt;b&gt;空格键&lt;/b&gt; 添加碎片</translation>
    </message>
    <message>
        <location filename="../qml/ScrapInteraction.qml" line="90"/>
        <source>Trace a cave section by &lt;b&gt;clicking&lt;/b&gt; points around it.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>SelectExportAreaTool</name>
    <message>
        <location filename="../qml/SelectExportAreaTool.qml" line="58"/>
        <source>Select Area</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/SelectExportAreaTool.qml" line="88"/>
        <source>Done</source>
        <translation type="unfinished">完成</translation>
    </message>
</context>
<context>
    <name>ShotDistanceDataBox</name>
    <message>
        <location filename="../qml/ShotDistanceDataBox.qml" line="38"/>
        <source>Excluded</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>SizeEditor</name>
    <message>
        <location filename="../qml/SizeEditor.qml" line="23"/>
        <source>Width</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/SizeEditor.qml" line="34"/>
        <source>x</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/SizeEditor.qml" line="39"/>
        <source>Height</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>StationBox</name>
    <message>
        <source>Good?</source>
        <translation type="obsolete">好？</translation>
    </message>
    <message>
        <location filename="../qml/StationBox.qml" line="72"/>
        <source>Press Enter</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>SurveyEditor</name>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="134"/>
        <source>Data</source>
        <translation type="unfinished">数据</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="103"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="160"/>
        <source>m</source>
        <translation type="unfinished">米</translation>
    </message>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="163"/>
        <source>ft</source>
        <translation type="unfinished">英尺</translation>
    </message>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="167"/>
        <source>Total Length: </source>
        <translation type="unfinished">总长度</translation>
    </message>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="181"/>
        <source>Press &lt;b&gt;Space&lt;/b&gt; to add another data block</source>
        <translation type="unfinished">按&lt;b&gt;空格键&lt;/b&gt; 添加另一组数据</translation>
    </message>
    <message>
        <location filename="../qml/SurveyEditor.qml" line="195"/>
        <source>Add Survey Data</source>
        <translation type="unfinished">添加测量数据</translation>
    </message>
</context>
<context>
    <name>TapeCalibrationEditor</name>
    <message>
        <location filename="../qml/TapeCalibrationEditor.qml" line="19"/>
        <source>Distance</source>
        <translation type="unfinished">距离</translation>
    </message>
    <message>
        <location filename="../qml/TapeCalibrationEditor.qml" line="38"/>
        <source>Calibration</source>
        <translation type="unfinished">校准</translation>
    </message>
    <message>
        <location filename="../qml/TapeCalibrationEditor.qml" line="55"/>
        <source>Units</source>
        <translation type="unfinished">单位</translation>
    </message>
    <message>
        <location filename="../qml/TapeCalibrationEditor.qml" line="88"/>
        <source>&lt;p&gt;Distance calibration allow you to correct a tape that&apos;s too long
or too short.  The calibration is added to uncorrected value (the value you read off the tape) to find the true value.&lt;/p&gt;
&lt;b&gt;UncorrectedValue + Calibration = TrueValue&lt;/b&gt;
&lt;p&gt; For example, the reading in the cave was 10m. You have a tape that&apos;s a 1m short. The true measured length
is 9m.  So you would enter -1 for the calibration. UncorrectedValue = 10m, Calibration = -1m, so 10m + (-1m) = 9m &lt;/p&gt;</source>
        <translation type="unfinished">&lt;p&gt;你可以通过距离校准来修正过长或过短的测尺
 将校准值与未修正值(你读到的偏差值)相加得到修正值。&lt;/p&gt;
&lt;b&gt;未修正值 + 校准值 = 修正值&lt;/b&gt;
&lt;p&gt; 例如, 你在洞内的读数为 10m. 但你的测尺短了 1m . 真正测量的长度
是 9m.  因此，你要减去 -1 . 未修正值 = 10m, 校准值 = -1m, 因此 10m + (-1m) = 9m &lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../qml/TapeCalibrationEditor.qml" line="100"/>
        <source>Tape&apos;s measurement units: meters (m) or feet (ft)</source>
        <translation type="unfinished">长度测量单位: 米 (m) 或 英尺 (ft)</translation>
    </message>
</context>
<context>
    <name>TeamTable</name>
    <message>
        <location filename="../qml/TeamTable.qml" line="30"/>
        <source>Team</source>
        <translation type="unfinished">团队</translation>
    </message>
    <message>
        <location filename="../qml/TeamTable.qml" line="59"/>
        <source>Name</source>
        <translation type="unfinished">名称</translation>
    </message>
    <message>
        <location filename="../qml/TeamTable.qml" line="70"/>
        <location filename="../qml/TeamTable.qml" line="287"/>
        <source>Role</source>
        <translation type="unfinished">角色</translation>
    </message>
    <message>
        <location filename="../qml/TeamTable.qml" line="326"/>
        <source>Add a team member</source>
        <translation type="unfinished">添加队员</translation>
    </message>
</context>
<context>
    <name>TripTabWidget</name>
    <message>
        <source>Overview</source>
        <translation type="obsolete">概览</translation>
    </message>
    <message>
        <source>This is the Trip overview page</source>
        <translation type="obsolete">航程概览页面</translation>
    </message>
    <message>
        <location filename="../qml/TripTabWidget.qml" line="24"/>
        <source>Data</source>
        <translation type="unfinished">数据</translation>
    </message>
    <message>
        <location filename="../qml/TripTabWidget.qml" line="32"/>
        <source>Notes</source>
        <translation type="unfinished">备注</translation>
    </message>
    <message>
        <source>Pictures</source>
        <translation type="obsolete">图片</translation>
    </message>
    <message>
        <source>This is the Pictures page</source>
        <translation type="obsolete">图片页面</translation>
    </message>
    <message>
        <source>Log</source>
        <translation type="obsolete">日志</translation>
    </message>
    <message>
        <source>This is the Log page</source>
        <translation type="obsolete">日志页面</translation>
    </message>
</context>
<context>
    <name>UnknownPage</name>
    <message>
        <location filename="../qml/UnknownPage.qml" line="22"/>
        <source>Unknown page!</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../qml/UnknownPage.qml" line="42"/>
        <source>Back</source>
        <translation type="unfinished">返回</translation>
    </message>
</context>
<context>
    <name>UsedStationsWidget</name>
    <message>
        <location filename="../qml/UsedStationsWidget.qml" line="61"/>
        <source>Used Stations</source>
        <translation type="unfinished">已用测点</translation>
    </message>
</context>
<context>
    <name>cwImportSurvexDialog</name>
    <message>
        <source>Dialog</source>
        <translation type="obsolete">对话</translation>
    </message>
    <message>
        <source>Survex Errors</source>
        <translation type="obsolete">测量错误</translation>
    </message>
    <message>
        <source>Import Errors</source>
        <translation type="obsolete">输入错误</translation>
    </message>
    <message>
        <source>TextLabel</source>
        <translation type="obsolete">文本标签</translation>
    </message>
    <message>
        <source>Import</source>
        <translation type="obsolete">输入</translation>
    </message>
</context>
<context>
    <name>cwMainWindow</name>
    <message>
        <source>Cave-vapor-where - Version: Super-ultra-alpha-3000 - Don&apos;t redistribute without permission :D</source>
        <translation type="obsolete">Cave-vapor-where - 版本: Super-ultra-alpha-3000 - 未经允许，不得转发n :D</translation>
    </message>
    <message>
        <source>File</source>
        <translation type="obsolete">文件</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation type="obsolete">编辑</translation>
    </message>
    <message>
        <source>Run</source>
        <translation type="obsolete">运行</translation>
    </message>
    <message>
        <source>Quit</source>
        <translation type="obsolete">停止</translation>
    </message>
    <message>
        <source>Import Survex</source>
        <translation type="obsolete">输入测量</translation>
    </message>
    <message>
        <source>Undo</source>
        <translation type="obsolete">取消</translation>
    </message>
    <message>
        <source>Ctrl+Z</source>
        <translation type="obsolete">还原</translation>
    </message>
    <message>
        <source>Redo</source>
        <translation type="obsolete">重做</translation>
    </message>
    <message>
        <source>Ctrl+Shift+Z</source>
        <translation type="obsolete">前进</translation>
    </message>
    <message>
        <source>ReloadQML</source>
        <translation type="obsolete">上载QML</translation>
    </message>
    <message>
        <source>Ctrl+R</source>
        <translation type="obsolete">右对齐</translation>
    </message>
    <message>
        <source>Save</source>
        <translation type="obsolete">存入</translation>
    </message>
    <message>
        <source>Compute Scraps</source>
        <translation type="obsolete">电脑碎片</translation>
    </message>
    <message>
        <source>Save As</source>
        <translation type="obsolete">另存为</translation>
    </message>
    <message>
        <source>Open</source>
        <translation type="obsolete">打开</translation>
    </message>
    <message>
        <source>New</source>
        <translation type="obsolete">新建</translation>
    </message>
</context>
<context>
    <name>cwTaskProgressDialog</name>
    <message>
        <source>Progress</source>
        <translation type="obsolete">进度</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation type="obsolete">结束</translation>
    </message>
</context>
</TS>
