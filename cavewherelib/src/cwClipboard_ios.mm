#include <QString>

#import <UIKit/UIKit.h>

void cwCopyTextToPasteboard(const QString &text)
{
    [UIPasteboard generalPasteboard].string = text.toNSString();
}
