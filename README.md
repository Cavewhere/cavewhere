Working known devices / digitizers

| OS                                        | Device                    | Pen                    | Notes                                                                                                                                                      |
|-------------------------------------------|---------------------------|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| iOS                                       | iPad Pro 10.5 (2017)      | Apple Pencil (1st gen) | __Works!__                                                                                                                                                     |
| Android                                   | Samsung Tab               | S Pen                  | __Works!__  
| macOS and SideCar                         | iPad Pro 10.5 (2017)      | Apple Pencil (1st gen) | Pressure sensitivity supported. Apple Pencil isn't detected as a stylus in Qt, but works with `PointerDevice.Unknown`. [QTBUG-80072](https://bugreports.qt.io/browse/QTBUG-80072) |
| __Setups that DON'T work__
| macOS                                     | ThinkPad T15m             | Pen                    | Fails. – Pen tracks, but pen tip doesn’t draw. Problem exists across all applications.                                                              |
| Windows 11 (ARM) via Parallels on macOs   | ThinkPad T15m             | Pen                    | Fails. – Pen tracks, but pen tip doesn’t draw. Problem exists across all applications.                                                              |
