/***************************************************************************
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Gunnar Sletta <gunnar.sletta@jollamobile.com>
**
** This file is part of lipstick.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#ifndef HWCRENDERSTAGE
#define HWCRENDERSTAGE

#include <private/qquickwindow_p.h>

Q_DECLARE_LOGGING_CATEGORY(LIPSTICK_LOG_HWC)

class LipstickCompositor;

namespace HwcInterface {
    class Compositor;
    class LayerList;
}

class HwcNode : public QSGNode
{
public:
    HwcNode();

    QRect bounds() const;

    void update(QSGGeometryNode *node, void *handle);

    void *handle() const { return m_buffer_handle; }
    bool isSubtreeBlocked() const { return m_blocked; }
    void setBlocked(bool b);

    float x() const { return m_x; }
    float y() const { return m_y; }
    void setPos(float x, float y) {
        m_x = x;
        m_y = y;
    }

    QSGGeometryNode *contentNode() const { return m_contentNode; }

private:
    QSGGeometryNode *m_contentNode;
    void *m_buffer_handle;
    float m_x, m_y;
    bool m_blocked;
};

class HwcRenderStage : public QObject, public QQuickCustomRenderStage
{
    Q_OBJECT

public:
    HwcRenderStage(LipstickCompositor *lipstick, void *hwcHandle);
    ~HwcRenderStage();
    bool render() Q_DECL_OVERRIDE;
    bool swap() Q_DECL_OVERRIDE;

    static void initialize(LipstickCompositor *lipstick);
    static bool isHwcEnabled() { return m_hwcEnabled; }

private:
    bool checkSceneGraph(QSGNode *node);

    LipstickCompositor *m_lipstick;
    QQuickWindow *m_window;

    HwcInterface::Compositor *m_hwc;

    QVector<HwcNode *> m_nodesInList;
    QVector<HwcNode *> m_nodesToTry;

    HwcInterface::LayerList *m_layerList;

    bool m_scheduledLayerList;

    static bool m_hwcEnabled;
};

#endif // HWCRENDERSTAGE