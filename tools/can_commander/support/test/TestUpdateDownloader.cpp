#include "tools/can_commander/support/UpdateDownloader.hpp"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

namespace
{
    using namespace tool;

    class TestUpdateDownloader
        : public testing::Test
    {
    public:
        static void SetUpTestSuite()
        {
            static int argc{ 1 };
            static char name[]{ "support_test" };
            static char* argv[]{ name, nullptr };

            if (QCoreApplication::instance() == nullptr)
                application = std::make_unique<QCoreApplication>(argc, argv);
        }

        static void TearDownTestSuite()
        {
            application.reset();
        }

        InstallerAsset Asset(const QByteArray& content, const QString& fileName = "e_foc-can_commander-v9.9.9-linux-x86_64.deb")
        {
            const auto path = QDir{ source.path() }.filePath("published.bin");
            QFile file{ path };
            EXPECT_TRUE(file.open(QIODevice::WriteOnly));
            file.write(content);
            return InstallerAsset{ fileName, QUrl::fromLocalFile(path), content.size(), QCryptographicHash::hash(content, QCryptographicHash::Sha256) };
        }

        void Download(const InstallerAsset& asset)
        {
            QEventLoop loop;
            QObject::connect(&downloader, &UpdateDownloader::Progress, &loop, [this](qint64 received, qint64 total)
                {
                    lastReceived = received;
                    lastTotal = total;
                });
            QObject::connect(&downloader, &UpdateDownloader::Downloaded, &loop, [this, &loop](const QString& path)
                {
                    downloaded = path;
                    loop.quit();
                });
            QObject::connect(&downloader, &UpdateDownloader::Failed, &loop, [this, &loop](const QString& reason)
                {
                    failure = reason;
                    loop.quit();
                });
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);

            downloader.Start(asset, target.path());
            loop.exec();
            QCoreApplication::processEvents();
        }

        QStringList TargetFiles() const
        {
            return QDir{ target.path() }.entryList(QDir::Files);
        }

        static QByteArray Read(const QString& path)
        {
            QFile file{ path };
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        }

        static inline std::unique_ptr<QCoreApplication> application;
        QTemporaryDir source;
        QTemporaryDir target;
        UpdateDownloader downloader;
        std::optional<QString> downloaded;
        std::optional<QString> failure;
        qint64 lastReceived{ -1 };
        qint64 lastTotal{ -1 };
        const QByteArray payload{ QByteArray{ "installer payload " }.repeated(512) };
    };
}

TEST_F(TestUpdateDownloader, downloads_and_verifies_the_installer)
{
    Download(Asset(payload));

    ASSERT_TRUE(downloaded.has_value()) << failure.value_or("").toStdString();
    EXPECT_EQ(QDir{ target.path() }.filePath("e_foc-can_commander-v9.9.9-linux-x86_64.deb"), *downloaded);
    EXPECT_EQ(payload, Read(*downloaded));
    EXPECT_EQ(QStringList{ "e_foc-can_commander-v9.9.9-linux-x86_64.deb" }, TargetFiles());
    EXPECT_EQ(payload.size(), lastReceived);
    EXPECT_EQ(payload.size(), lastTotal);
    EXPECT_FALSE(downloader.IsDownloading());
}

TEST_F(TestUpdateDownloader, deletes_a_download_that_does_not_match_the_checksum)
{
    auto asset = Asset(payload);
    asset.sha256 = QCryptographicHash::hash("something else", QCryptographicHash::Sha256);

    Download(asset);

    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ("the download does not match the checksum published with the release", *failure);
    EXPECT_FALSE(downloaded.has_value());
    EXPECT_TRUE(TargetFiles().isEmpty());
}

TEST_F(TestUpdateDownloader, deletes_a_download_of_unexpected_size)
{
    auto asset = Asset(payload);
    asset.size = payload.size() + 1;

    Download(asset);

    ASSERT_TRUE(failure.has_value());
    EXPECT_TRUE(failure->contains("bytes")) << failure->toStdString();
    EXPECT_TRUE(TargetFiles().isEmpty());
}

TEST_F(TestUpdateDownloader, refuses_an_installer_without_a_published_checksum)
{
    auto asset = Asset(payload);
    asset.sha256.reset();

    Download(asset);

    ASSERT_TRUE(failure.has_value());
    EXPECT_TRUE(failure->contains("no checksum")) << failure->toStdString();
    EXPECT_TRUE(TargetFiles().isEmpty());
}

TEST_F(TestUpdateDownloader, refuses_an_installer_name_with_a_path)
{
    Download(Asset(payload, "../e_foc-can_commander-v9.9.9-linux-x86_64.deb"));

    ASSERT_TRUE(failure.has_value());
    EXPECT_TRUE(failure->contains("not a plain file name")) << failure->toStdString();
    EXPECT_TRUE(TargetFiles().isEmpty());
}

TEST_F(TestUpdateDownloader, reports_an_unreachable_installer)
{
    auto asset = Asset(payload);
    asset.url = QUrl::fromLocalFile(QDir{ source.path() }.filePath("missing.bin"));

    Download(asset);

    ASSERT_TRUE(failure.has_value());
    EXPECT_FALSE(failure->isEmpty());
    EXPECT_TRUE(TargetFiles().isEmpty());
}

TEST_F(TestUpdateDownloader, replaces_an_earlier_download_of_the_same_installer)
{
    QFile stale{ QDir{ target.path() }.filePath("e_foc-can_commander-v9.9.9-linux-x86_64.deb") };
    ASSERT_TRUE(stale.open(QIODevice::WriteOnly));
    stale.write("stale");
    stale.close();

    Download(Asset(payload));

    ASSERT_TRUE(downloaded.has_value()) << failure.value_or("").toStdString();
    EXPECT_EQ(payload, Read(*downloaded));
}

TEST_F(TestUpdateDownloader, cancel_discards_the_partial_download_silently)
{
    bool signalled{ false };
    QObject::connect(&downloader, &UpdateDownloader::Downloaded, [&signalled](const QString&)
        {
            signalled = true;
        });
    QObject::connect(&downloader, &UpdateDownloader::Failed, [&signalled](const QString&)
        {
            signalled = true;
        });

    downloader.Start(Asset(payload), target.path());
    EXPECT_TRUE(downloader.IsDownloading());
    downloader.Cancel();

    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();

    EXPECT_FALSE(signalled);
    EXPECT_FALSE(downloader.IsDownloading());
    EXPECT_TRUE(TargetFiles().isEmpty());
}
