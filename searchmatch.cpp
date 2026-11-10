#include "searchmatch.h"

#include "common.h"

bool SearchMatch::matchesFileLabels(const QSet<QString> &setFileLabels,
                                    const QList<QSet<QString>> &qLLabelGroups,
                                    int nLogic)
{
    // 没有选择任何标签时不该命中任何文件
    if (qLLabelGroups.isEmpty())
        return false;
    // 文件上一个标签都没有，肯定不匹配（也省掉后面的循环）
    if (setFileLabels.isEmpty())
        return false;

    bool bResult = (nLogic == labelLogic::AND);

    if (nLogic != labelLogic::AND) {
        // 组间 OR：任意一组里有任意一个标签命中即可
        for (const QSet<QString> &setGroup : qLLabelGroups) {
            for (QSet<QString>::const_iterator cItr = setGroup.cbegin(); cItr != setGroup.cend(); ++cItr) {
                if (setFileLabels.contains(*cItr))
                    return true;
            }
        }
        return false;
    }

    // 组间 AND：每一组都必须至少命中一个标签
    for (const QSet<QString> &setGroup : qLLabelGroups) {
        bool bHit = false;
        for (QSet<QString>::const_iterator cItr = setGroup.cbegin(); cItr != setGroup.cend(); ++cItr) {
            if (setFileLabels.contains(*cItr)) {
                bHit = true;
                break;
            }
        }
        if (!bHit)
            return false;
    }
    return bResult;
}
