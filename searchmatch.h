/**
** 搜索匹配规则的唯一实现。
**
** 同一套"组"语义在两条路径上都要用：
**   - 磁盘扫描：ADSOperation::isHostFile() 逐个文件读 ADS 标签；
**   - 缓存查询：SqliteOperation::searchFilesByLabels() 从 sqlite 里读标签串。
** 以前两处各写了一份判断，规则一旦改动就会漂移（同一查询"缓存"与"重扫"结果不同），
** 所以统一到这里，两边都只负责"把标签拿出来"，判断交给 matchesFileLabels()。
**/

#ifndef SEARCHMATCH_H
#define SEARCHMATCH_H

#include <QList>
#include <QSet>
#include <QString>

namespace SearchMatch {

/**
 * 一个文件是否满足搜索条件。
 *
 * 条件由若干"标签组"组成：每组 = 用户在树里选中的一个标签 + 它的全部子标签。
 *   - 组内是 OR：文件只要打上组里任意一个标签就算这组命中；
 *   - 组间按 nLogic 组合：AND 要求每组都命中，OR 只要有一组命中；
 *   - 空条件（没有任何组）视为不匹配任何文件，否则空集合会被误判成"全部满足"。
 *
 * @param setFileLabels 文件上的标签集合（调用方负责过滤掉第三方 ADS）
 * @param qLLabelGroups 标签组
 * @param nLogic        labelLogic::AND 或 labelLogic::OR
 */
bool matchesFileLabels(const QSet<QString> &setFileLabels,
                       const QList<QSet<QString>> &qLLabelGroups,
                       int nLogic);

}   // namespace SearchMatch

#endif // SEARCHMATCH_H
