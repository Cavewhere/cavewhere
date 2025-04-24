Working known devices / digitizers

| OS                                        | Device                    | Pen                    | Notes                                                                                                                                                      |
|-------------------------------------------|---------------------------|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| iOS                                       | iPad Pro 10.5 (2017)      | Apple Pencil (1st gen) | __Works!__                                                                                                                                                     |
| Android                                   | Samsung Note 10           | S Pen                  | __Works!__  
| Windows 11                                | ThinkVision M14t (Gen 1)  | Pen                    | __Works!__  
| macOS and SideCar                         | iPad Pro 10.5 (2017)      | Apple Pencil (1st gen) | Pressure sensitivity supported. Apple Pencil isn't detected as a stylus in Qt, but works with `PointerDevice.Unknown`. [QTBUG-80072](https://bugreports.qt.io/browse/QTBUG-80072) |
| __Setups that DON'T work__
| Android < 9.0                             | Samsung Note 4            | S Pen                  | Uses Android 6.0, which is too old and unsupported by Qt 6.8.3 (needs android >= 9)                                                                |
| macOS                                     | ThinkVision M14t (Gen 1)  | Pen                    | Fails. – Pen tracks, but pen tip doesn’t draw. Problem exists across all applications.                                                              |
| Windows 11 (ARM) via Parallels on macOs   | ThinkVision M14t (Gen 1)  | Pen                    | Fails. – Pen tracks, but pen tip doesn’t draw. Problem exists across all applications.                                                              |
