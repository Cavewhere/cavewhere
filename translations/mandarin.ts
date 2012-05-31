<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="zh_CN" sourcelanguage="en_US">
<context>
    <name>AllCavesTabWidget</name>
    <message>
        <location filename="qml/AllCavesTabWidget.qml" line="5"/>
        <source>Overview</source>
        <translation type="unfinished">概览</translation>
    </message>
    <message>
        <location filename="qml/AllCavesTabWidget.qml" line="7"/>
        <source>This is the overview of all the caves</source>
        <translation type="unfinished">洞穴概览</translation>
    </message>
    <message>
        <location filename="qml/AllCavesTabWidget.qml" line="11"/>
        <source>Connections</source>
        <translation type="unfinished">连接</translation>
    </message>
    <message>
        <location filename="qml/AllCavesTabWidget.qml" line="13"/>
        <source>This is the Connection page</source>
        <translation type="unfinished">连接页面</translation>
    </message>
</context>
<context>
    <name>BackSightCalibrationEditor</name>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="13"/>
        <source>&lt;b&gt;Back Sights&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;后视&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="36"/>
        <source>Compass calibration</source>
        <translation type="unfinished">方位校准</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="54"/>
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
        <location filename="qml/BackSightCalibrationEditor.qml" line="62"/>
        <source>Clino calibration</source>
        <translation type="unfinished">倾角校准</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="80"/>
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
        <location filename="qml/BackSightCalibrationEditor.qml" line="95"/>
        <source>Corrected &lt;i&gt;Compass&lt;/i&gt;</source>
        <translation type="unfinished">修正 &lt;i&gt;方位&lt;/i&gt;</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="110"/>
        <source>Corrected compass allow you to entry back sights as if they were read as
            a front sight.  This will &lt;b&gt;subtract 180°&lt;/b&gt; to all back sight compass readings to get
            the true value.</source>
        <translation type="unfinished">你可以输入后视修正方位，就如你
            前视一样。  你要 &lt;b&gt;减去 180°&lt;/b&gt; 从而得到
            修正值.</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="123"/>
        <source>Corrected &lt;i&gt;Clino&lt;/i&gt;</source>
        <translation type="unfinished">修正 &lt;i&gt;倾角&lt;/i&gt;</translation>
    </message>
    <message>
        <location filename="qml/BackSightCalibrationEditor.qml" line="137"/>
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
        <location filename="qml/BackSightReadingBox.qml" line="6"/>
        <source>bs</source>
        <translation type="unfinished">后视</translation>
    </message>
</context>
<context>
    <name>CalibrationEditor</name>
    <message>
        <location filename="qml/CalibrationEditor.qml" line="26"/>
        <source>Calibration</source>
        <translation type="unfinished">校准</translation>
    </message>
    <message>
        <location filename="qml/CalibrationEditor.qml" line="80"/>
        <source>Hmm, you need to &lt;b&gt;check&lt;/b&gt; either &lt;i&gt;front&lt;/i&gt; or &lt;i&gt;back sights&lt;/i&gt; box, or both, depending on your data.</source>
        <translation type="unfinished">嗯，依据你的数据，你需要 &lt;b&gt;检查&lt;/b&gt; 或者 &lt;i&gt;前视&lt;/i&gt; 或者 &lt;i&gt;后视&lt;/i&gt; 读数, 或者两个都检查。</translation>
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
        <location filename="qml/CaveDataSidebarPage.qml" line="77"/>
        <source>Only 1 Cave</source>
        <translation type="unfinished">单个洞穴</translation>
    </message>
    <message>
        <location filename="qml/CaveDataSidebarPage.qml" line="84"/>
        <source>Add Cave</source>
        <translation type="unfinished">添加洞穴</translation>
    </message>
    <message>
        <location filename="qml/CaveDataSidebarPage.qml" line="105"/>
        <source>Add Trip</source>
        <translation type="unfinished">添加航程</translation>
    </message>
</context>
<context>
    <name>CaveTabWidget</name>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="7"/>
        <source>Overview</source>
        <translation type="unfinished">概览</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="12"/>
        <source>Notes</source>
        <translation type="unfinished">备注</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="14"/>
        <source>This is the Notes page</source>
        <translation type="unfinished">备注页面</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="18"/>
        <source>Team</source>
        <translation type="unfinished">团队</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="20"/>
        <source>This is the Team page</source>
        <translation type="unfinished">团队页面</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="24"/>
        <source>Pictures</source>
        <translation type="unfinished">图片</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="26"/>
        <source>This is the Pictures page</source>
        <translation type="unfinished">图片页面</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="30"/>
        <source>Log</source>
        <translation type="unfinished">日志</translation>
    </message>
    <message>
        <location filename="qml/CaveTabWidget.qml" line="32"/>
        <source>This is the Log page</source>
        <translation type="unfinished">日志页面</translation>
    </message>
</context>
<context>
    <name>CaveTreeDelegate</name>
    <message>
        <location filename="qml/CaveTreeDelegate.qml" line="55"/>
        <source>Remove</source>
        <translation type="unfinished">移动</translation>
    </message>
    <message>
        <location filename="qml/CaveTreeDelegate.qml" line="64"/>
        <source>Cancel</source>
        <translation type="unfinished">取消</translation>
    </message>
    <message>
        <location filename="qml/CaveTreeDelegate.qml" line="181"/>
        <source>Trips</source>
        <translation type="unfinished">航程</translation>
    </message>
</context>
<context>
    <name>DataSideBar</name>
    <message>
        <location filename="qml/DataSideBar.qml" line="28"/>
        <source>Caves</source>
        <translation type="unfinished">洞穴</translation>
    </message>
    <message>
        <location filename="qml/DataSideBar.qml" line="33"/>
        <source>Surface</source>
        <translation type="unfinished">地表</translation>
    </message>
    <message>
        <location filename="qml/DataSideBar.qml" line="35"/>
        <source>This is the Surface page</source>
        <translation type="unfinished">地表页面</translation>
    </message>
</context>
<context>
    <name>DeclainationEditor</name>
    <message>
        <location filename="qml/DeclainationEditor.qml" line="11"/>
        <location filename="qml/DeclainationEditor.qml" line="23"/>
        <source>Declination</source>
        <translation type="unfinished">磁偏角</translation>
    </message>
    <message>
        <location filename="qml/DeclainationEditor.qml" line="39"/>
        <source>&lt;p&gt;Magnetic declination is the &lt;b&gt;angle between magnetic north and true north&lt;/b&gt;&lt;/p&gt;
            Cavewhere calculates the true bearing (&lt;b&gt;TB&lt;/b&gt;) by adding declination (&lt;b&gt;D&lt;/b&gt;) to magnetic bearing (&lt;b&gt;MB&lt;/b&gt;).
            &lt;center&gt;&lt;b&gt;MB + D = TB&lt;/b&gt;&lt;/center&gt;</source>
        <translation type="unfinished">&lt;p&gt;磁偏角是 &lt;b&gt;真北和磁北之间的角度差&lt;/b&gt;&lt;/p&gt;
            Cavewhere 通过  将磁偏角 (&lt;b&gt;D&lt;/b&gt;) 与磁北方位角 (&lt;b&gt;MB&lt;/b&gt;).相加得到真北方位角(&lt;b&gt;TB&lt;/b&gt;)
            &lt;center&gt;&lt;b&gt;MB + D = TB&lt;/b&gt;&lt;/center&gt;</translation>
    </message>
</context>
<context>
    <name>FrontSightCalibrationEditor</name>
    <message>
        <location filename="qml/FrontSightCalibrationEditor.qml" line="13"/>
        <source>&lt;b&gt;Front Sights&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;前视&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="qml/FrontSightCalibrationEditor.qml" line="34"/>
        <source>Compass calibration</source>
        <translation type="unfinished">方位校准</translation>
    </message>
    <message>
        <location filename="qml/FrontSightCalibrationEditor.qml" line="52"/>
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
        <location filename="qml/FrontSightCalibrationEditor.qml" line="60"/>
        <source>Clino calibration</source>
        <translation type="unfinished">倾角校准</translation>
    </message>
    <message>
        <location filename="qml/FrontSightCalibrationEditor.qml" line="80"/>
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
        <location filename="qml/FrontSightReadingBox.qml" line="5"/>
        <source>fs</source>
        <translation type="unfinished">前视</translation>
    </message>
</context>
<context>
    <name>HelpArea</name>
    <message>
        <location filename="qml/HelpArea.qml" line="53"/>
        <source>No documentation</source>
        <translation type="unfinished">暂无文本</translation>
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
    <name>NoteNorthInteraction</name>
    <message>
        <location filename="qml/NoteNorthInteraction.qml" line="62"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the north arrow&apos;s first point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 指北针第一点</translation>
    </message>
    <message>
        <location filename="qml/NoteNorthInteraction.qml" line="95"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the north arrow&apos;s second point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 指北针第二点</translation>
    </message>
</context>
<context>
    <name>NoteNorthUpInput</name>
    <message>
        <location filename="qml/NoteNorthUpInput.qml" line="30"/>
        <source>North</source>
        <translation type="unfinished">北</translation>
    </message>
    <message>
        <location filename="qml/NoteNorthUpInput.qml" line="37"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="qml/NoteNorthUpInput.qml" line="45"/>
        <source>&amp;deg</source>
        <translation type="unfinished">&amp;度数</translation>
    </message>
</context>
<context>
    <name>NoteScaleInput</name>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="37"/>
        <source>Scale</source>
        <translation type="unfinished">比例尺</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="56"/>
        <source>On Paper</source>
        <translation type="unfinished">纸上</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="71"/>
        <location filename="qml/NoteScaleInput.qml" line="106"/>
        <source>=</source>
        <translation type="unfinished">=</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="89"/>
        <source>In Cave</source>
        <translation type="unfinished">洞内</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="113"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="121"/>
        <source>Weird scaling units</source>
        <translation type="unfinished">比例尺单位</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInput.qml" line="145"/>
        <source>1:</source>
        <translation type="unfinished">1:</translation>
    </message>
</context>
<context>
    <name>NoteScaleInteraction</name>
    <message>
        <location filename="qml/NoteScaleInteraction.qml" line="86"/>
        <source>&lt;b&gt;In cave length&lt;/b&gt;</source>
        <translation type="unfinished">&lt;b&gt;洞内长度&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInteraction.qml" line="97"/>
        <source>Done</source>
        <translation type="unfinished">完成</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInteraction.qml" line="119"/>
        <source>&lt;b&gt;Click&lt;/b&gt; the length&apos;s first point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 长度第一点</translation>
    </message>
    <message>
        <location filename="qml/NoteScaleInteraction.qml" line="154"/>
        <source>&lt;b&gt; Click &lt;/b&gt; the length&apos;s second point</source>
        <translation type="unfinished">&lt;b&gt;点击&lt;/b&gt; 长度第二点</translation>
    </message>
</context>
<context>
    <name>NoteStationInteraction</name>
    <message>
        <location filename="qml/NoteStationInteraction.qml" line="35"/>
        <source>Click to add new station</source>
        <translation type="unfinished">点击添加测点</translation>
    </message>
</context>
<context>
    <name>NoteTransformEditor</name>
    <message>
        <location filename="qml/NoteTransformEditor.qml" line="43"/>
        <source>Scrap Info</source>
        <translation type="unfinished">小块信息</translation>
    </message>
    <message>
        <location filename="qml/NoteTransformEditor.qml" line="79"/>
        <source>Auto Calculate</source>
        <translation type="unfinished">自动计算</translation>
    </message>
    <message>
        <location filename="qml/NoteTransformEditor.qml" line="106"/>
        <source>You can set the direction of &lt;b&gt;north&lt;/b&gt; relative to page for a scrap.
            Cavewhere only uses &lt;b&gt;north&lt;/b&gt; to help you automatically label stations.</source>
        <translation type="unfinished">你可相对纸张设置小块指 &lt;b&gt;北&lt;/b&gt; 向。
            Cavewhere 只用 &lt;b&gt;北&lt;/b&gt; 帮助你自动标注测点.</translation>
    </message>
    <message>
        <location filename="qml/NoteTransformEditor.qml" line="120"/>
        <source>You can set the &lt;b&gt;scale&lt;/b&gt; of the scrap.</source>
        <translation type="unfinished">你可设置小块&lt;b&gt;比例尺&lt;/b&gt;.</translation>
    </message>
</context>
<context>
    <name>NotesGallery</name>
    <message>
        <location filename="qml/NotesGallery.qml" line="190"/>
        <source>Rotate</source>
        <translation type="unfinished">旋转</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="204"/>
        <source>Carpet</source>
        <translation type="unfinished">覆盖</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="214"/>
        <source>Load</source>
        <translation type="unfinished">载入</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="249"/>
        <source>Back</source>
        <translation type="unfinished">返回</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="261"/>
        <source>Select</source>
        <translation type="unfinished">选择</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="270"/>
        <source>Add</source>
        <translation type="unfinished">添加</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="276"/>
        <source>Station</source>
        <translation type="unfinished">测点</translation>
    </message>
    <message>
        <location filename="qml/NotesGallery.qml" line="285"/>
        <source>Scrap</source>
        <translation type="unfinished">舍弃</translation>
    </message>
</context>
<context>
    <name>ScrapInteraction</name>
    <message>
        <location filename="qml/ScrapInteraction.qml" line="39"/>
        <source>Trace a scrap by &lt;b&gt;clicking&lt;/b&gt; points around it &lt;br&gt;
        Press &lt;b&gt;spacebar&lt;/b&gt; to add a new scrap</source>
        <translation type="unfinished">通过 &lt;b&gt;点击&lt;/b&gt; 碎片周围的点 &lt;br&gt;追踪碎片
        按 &lt;b&gt;空格键&lt;/b&gt; 添加碎片</translation>
    </message>
</context>
<context>
    <name>StationBox</name>
    <message>
        <location filename="qml/StationBox.qml" line="58"/>
        <source>Good?</source>
        <translation type="unfinished">好？</translation>
    </message>
</context>
<context>
    <name>SurveyEditor</name>
    <message>
        <location filename="qml/SurveyEditor.qml" line="82"/>
        <source>Data</source>
        <translation type="unfinished">数据</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="103"/>
        <source></source>
        <translation></translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="106"/>
        <source>m</source>
        <translation type="unfinished">米</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="109"/>
        <source>ft</source>
        <translation type="unfinished">英尺</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="113"/>
        <source>Total Length: </source>
        <translation type="unfinished">总长度</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="127"/>
        <source>Press &lt;b&gt;Space&lt;/b&gt; to add another data block</source>
        <translation type="unfinished">按&lt;b&gt;空格键&lt;/b&gt; 添加另一组数据</translation>
    </message>
    <message>
        <location filename="qml/SurveyEditor.qml" line="141"/>
        <source>Add Survey Data</source>
        <translation type="unfinished">添加测量数据</translation>
    </message>
</context>
<context>
    <name>TapeCalibrationEditor</name>
    <message>
        <location filename="qml/TapeCalibrationEditor.qml" line="12"/>
        <source>Distance</source>
        <translation type="unfinished">距离</translation>
    </message>
    <message>
        <location filename="qml/TapeCalibrationEditor.qml" line="31"/>
        <source>Calibration</source>
        <translation type="unfinished">校准</translation>
    </message>
    <message>
        <location filename="qml/TapeCalibrationEditor.qml" line="48"/>
        <source>Units</source>
        <translation type="unfinished">单位</translation>
    </message>
    <message>
        <location filename="qml/TapeCalibrationEditor.qml" line="81"/>
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
        <location filename="qml/TapeCalibrationEditor.qml" line="89"/>
        <source>Tape&apos;s measurement units: meters (m) or feet (ft)</source>
        <translation type="unfinished">长度测量单位: 米 (m) 或 英尺 (ft)</translation>
    </message>
</context>
<context>
    <name>TeamTable</name>
    <message>
        <location filename="qml/TeamTable.qml" line="23"/>
        <source>Team</source>
        <translation type="unfinished">团队</translation>
    </message>
    <message>
        <location filename="qml/TeamTable.qml" line="52"/>
        <source>Name</source>
        <translation type="unfinished">名称</translation>
    </message>
    <message>
        <location filename="qml/TeamTable.qml" line="63"/>
        <location filename="qml/TeamTable.qml" line="280"/>
        <source>Role</source>
        <translation type="unfinished">角色</translation>
    </message>
    <message>
        <location filename="qml/TeamTable.qml" line="319"/>
        <source>Add a team member</source>
        <translation type="unfinished">添加队员</translation>
    </message>
</context>
<context>
    <name>TripTabWidget</name>
    <message>
        <location filename="qml/TripTabWidget.qml" line="9"/>
        <source>Overview</source>
        <translation type="unfinished">概览</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="11"/>
        <source>This is the Trip overview page</source>
        <translation type="unfinished">航程概览页面</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="16"/>
        <source>Data</source>
        <translation type="unfinished">数据</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="24"/>
        <source>Notes</source>
        <translation type="unfinished">备注</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="48"/>
        <source>Pictures</source>
        <translation type="unfinished">图片</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="50"/>
        <source>This is the Pictures page</source>
        <translation type="unfinished">图片页面</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="54"/>
        <source>Log</source>
        <translation type="unfinished">日志</translation>
    </message>
    <message>
        <location filename="qml/TripTabWidget.qml" line="56"/>
        <source>This is the Log page</source>
        <translation type="unfinished">日志页面</translation>
    </message>
</context>
<context>
    <name>UsedStationsWidget</name>
    <message>
        <location filename="qml/UsedStationsWidget.qml" line="78"/>
        <source>Used Stations</source>
        <translation type="unfinished">已用测点</translation>
    </message>
</context>
<context>
    <name>cwImportSurvexDialog</name>
    <message>
        <location filename="src/cwImportSurvexDialog.ui" line="14"/>
        <source>Dialog</source>
        <translation type="unfinished">对话</translation>
    </message>
    <message>
        <location filename="src/cwImportSurvexDialog.ui" line="55"/>
        <source>Survex Errors</source>
        <translation type="unfinished">测量错误</translation>
    </message>
    <message>
        <location filename="src/cwImportSurvexDialog.ui" line="65"/>
        <source>Import Errors</source>
        <translation type="unfinished">输入错误</translation>
    </message>
    <message>
        <location filename="src/cwImportSurvexDialog.ui" line="92"/>
        <source>TextLabel</source>
        <translation type="unfinished">文本标签</translation>
    </message>
    <message>
        <location filename="src/cwImportSurvexDialog.ui" line="99"/>
        <source>Import</source>
        <translation type="unfinished">输入</translation>
    </message>
</context>
<context>
    <name>cwMainWindow</name>
    <message>
        <location filename="src/cwMainWindow.ui" line="14"/>
        <source>Cave-vapor-where - Version: Super-ultra-alpha-3000 - Don&apos;t redistribute without permission :D</source>
        <translation type="unfinished">Cave-vapor-where - 版本: Super-ultra-alpha-3000 - 未经允许，不得转发n :D</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="44"/>
        <source>File</source>
        <translation type="unfinished">文件</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="58"/>
        <source>Edit</source>
        <translation type="unfinished">编辑</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="65"/>
        <source>Run</source>
        <translation type="unfinished">运行</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="75"/>
        <source>Quit</source>
        <translation type="unfinished">停止</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="80"/>
        <source>Import Survex</source>
        <translation type="unfinished">输入测量</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="85"/>
        <source>Undo</source>
        <translation type="unfinished">取消</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="88"/>
        <source>Ctrl+Z</source>
        <translation type="unfinished">还原</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="93"/>
        <source>Redo</source>
        <translation type="unfinished">重做</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="96"/>
        <source>Ctrl+Shift+Z</source>
        <translation type="unfinished">前进</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="101"/>
        <source>ReloadQML</source>
        <translation type="unfinished">上载QML</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="104"/>
        <source>Ctrl+R</source>
        <translation type="unfinished">右对齐</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="109"/>
        <source>Save</source>
        <translation type="unfinished">存入</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="114"/>
        <source>Compute Scraps</source>
        <translation type="unfinished">电脑碎片</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="119"/>
        <source>Save As</source>
        <translation type="unfinished">另存为</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="124"/>
        <source>Open</source>
        <translation type="unfinished">打开</translation>
    </message>
    <message>
        <location filename="src/cwMainWindow.ui" line="129"/>
        <source>New</source>
        <translation type="unfinished">新建</translation>
    </message>
</context>
<context>
    <name>cwTaskProgressDialog</name>
    <message>
        <location filename="src/cwTaskProgressDialog.ui" line="14"/>
        <source>Progress</source>
        <translation type="unfinished">进度</translation>
    </message>
    <message>
        <location filename="src/cwTaskProgressDialog.ui" line="62"/>
        <source>Stop</source>
        <translation type="unfinished">结束</translation>
    </message>
</context>
</TS>
