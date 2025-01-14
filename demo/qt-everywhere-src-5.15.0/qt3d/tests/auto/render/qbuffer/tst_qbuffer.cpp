/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// TODO Remove in Qt6
#include <QtCore/qcompilerdetection.h>
QT_WARNING_DISABLE_DEPRECATED

#include <QtTest/QTest>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/private/qbuffer_p.h>
#include <Qt3DRender/qbufferdatagenerator.h>

#include "testpostmanarbiter.h"

class TestFunctor : public Qt3DRender::QBufferDataGenerator
{
public:
    explicit TestFunctor(int size)
        : m_size(size)
    {}

    QByteArray operator ()() final
    {
        return QByteArray();
    }

    bool operator ==(const Qt3DRender::QBufferDataGenerator &other) const final
    {
        const TestFunctor *otherFunctor = Qt3DRender::functor_cast<TestFunctor>(&other);
        if (otherFunctor != nullptr)
            return otherFunctor->m_size == m_size;
        return false;
    }

    QT3D_FUNCTOR(TestFunctor)

private:
    int m_size;
};

class tst_QBuffer: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QBuffer *>("buffer");

        Qt3DRender::QBuffer *defaultConstructed = new Qt3DRender::QBuffer();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        auto buffer = new Qt3DRender::QBuffer;
        buffer->setUsage(Qt3DRender::QBuffer::DynamicRead);
        buffer->setData(QByteArrayLiteral("There's no replacement"));
        buffer->setDataGenerator(Qt3DRender::QBufferDataGeneratorPtr(new TestFunctor(883)));
        QTest::newRow("vertex") << buffer;

        auto indexBuffer = new Qt3DRender::QBuffer;
        indexBuffer->setUsage(Qt3DRender::QBuffer::StaticCopy);
        indexBuffer->setData(QByteArrayLiteral("For displacement"));
        indexBuffer->setDataGenerator(Qt3DRender::QBufferDataGeneratorPtr(new TestFunctor(1340)));
        indexBuffer->setSyncData(true);
        QTest::newRow("index") << indexBuffer;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QBuffer *, buffer);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(buffer);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QBufferData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QBufferData>>(creationChanges.first());
        const Qt3DRender::QBufferData &cloneData = creationChangeData->data;


        QCOMPARE(buffer->id(), creationChangeData->subjectId());
        QCOMPARE(buffer->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(buffer->metaObject(), creationChangeData->metaObject());
        QCOMPARE(buffer->data(), cloneData.data);
        QCOMPARE(buffer->usage(), cloneData.usage);
        QCOMPARE(buffer->dataGenerator(), cloneData.functor);
        QCOMPARE(buffer->isSyncData(), cloneData.syncData);
        if (buffer->dataGenerator()) {
            QVERIFY(cloneData.functor);
            QVERIFY(*cloneData.functor == *buffer->dataGenerator());
            QCOMPARE((*cloneData.functor)(), (*buffer->dataGenerator())());
        }
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QBuffer> buffer(new Qt3DRender::QBuffer);
        arbiter.setArbiterOnNode(buffer.data());

        // WHEN
        buffer->setUsage(Qt3DRender::QBuffer::DynamicCopy);

        // THEN
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(arbiter.dirtyNodes.size(), 1);
        QCOMPARE(arbiter.dirtyNodes.front(), buffer.data());

        arbiter.dirtyNodes.clear();

        // WHEN
        buffer->setData(QByteArrayLiteral("Z28"));

        // THEN
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(arbiter.dirtyNodes.size(), 1);
        QCOMPARE(arbiter.dirtyNodes.front(), buffer.data());

        arbiter.dirtyNodes.clear();

        // WHEN
        Qt3DRender::QBufferDataGeneratorPtr functor(new TestFunctor(355));
        buffer->setDataGenerator(functor);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.dirtyNodes.size(), 1);
        QCOMPARE(arbiter.dirtyNodes.front(), buffer.data());

        arbiter.dirtyNodes.clear();

        // WHEN
        buffer->setSyncData(true);

        // THEN
        QCOMPARE(arbiter.events.size(), 0);
        QCOMPARE(arbiter.dirtyNodes.size(), 1);
        QCOMPARE(arbiter.dirtyNodes.front(), buffer.data());

        arbiter.dirtyNodes.clear();

        // WHEN
        buffer->updateData(1, QByteArrayLiteral("L1"));
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.dirtyNodes.size(), 1);
        QCOMPARE(arbiter.dirtyNodes.front(), buffer.data());
    }
};

QTEST_MAIN(tst_QBuffer)

#include "tst_qbuffer.moc"
