// TsundereGram Desktop: profile features shared with TsundereGram for Android.
// This file is compiled as a part of ayu/utils/rc_manager.cpp.
#include "tsundere/tsundere.h"

#include "tsundere/tsundere_secrets.h"
#include "tsundere/tsundere_badges.h"

#ifndef TSUNDERE_STANDALONE_CHECK
#include "data/data_peer.h"
#include "media/clip/media_clip_reader.h"
#include "ui/chat/attach/attach_prepare.h"
#include "settings.h"
#endif // TSUNDERE_STANDALONE_CHECK

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QVariant>
#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <openssl/evp.h>

#include <algorithm>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace Tsundere {
namespace {

const char kApiBaseUrl[] = "https://tsundere-api.tsunderegram.workers.dev";

struct BadgeEntry {
	BadgeKind kind = BadgeKind::None;
	QString text;
};

struct GradientEntry {
	QColor from;
	QColor to;
};

struct BannerEntry {
	QString url;
	QString type;
	bool hideAvatar = false;
};

struct Config {
	QHash<QString, BadgeEntry> badges;
	QHash<QString, GradientEntry> gradients;
	QHash<QString, BannerEntry> banners;
};

struct BannerImage {
	QImage original;
	bool loading = false;
	std::vector<std::function<void()>> waiters;
};

struct OwnerBanner {
	QString url;
	bool hideAvatar = false;
	QImage cache;
	QSize cacheSize;
};

Config &State() {
	static auto result = Config();
	return result;
}

std::unordered_map<qint64, BadgeKind> &BadgeMemo() {
	static auto result = std::unordered_map<qint64, BadgeKind>();
	return result;
}

std::unordered_map<qint64, std::optional<GradientEntry>> &GradientMemo() {
	static auto result
		= std::unordered_map<qint64, std::optional<GradientEntry>>();
	return result;
}

std::map<QString, BannerImage> &BannerImages() {
	static auto result = std::map<QString, BannerImage>();
	return result;
}

std::unordered_map<const QWidget*, OwnerBanner> &Owners() {
	static auto result = std::unordered_map<const QWidget*, OwnerBanner>();
	return result;
}

QByteArray Unmask(const unsigned char *data, std::size_t size) {
	auto result = QByteArray(int(size), char(0));
	const auto maskSize = sizeof(Secrets::kMask);
	for (std::size_t i = 0; i != size; ++i) {
		result[int(i)] = char(data[i] ^ Secrets::kMask[i % maskSize]);
	}
	return result;
}

const QByteArray &Salt() {
	static const auto result = Unmask(Secrets::kSalt, sizeof(Secrets::kSalt));
	return result;
}

const QByteArray &AesKey() {
	static const auto result = QCryptographicHash::hash(
		Unmask(Secrets::kPassphrase, sizeof(Secrets::kPassphrase)),
		QCryptographicHash::Sha256);
	return result;
}

// Same as TsundereCrypto.hashUserId on Android.
QString HashKey(const QString &key) {
	auto input = QByteArray();
	input.append(Salt());
	input.append(':');
	input.append(key.toUtf8());
	input.append(':');
	input.append(Salt());
	const auto hash = QCryptographicHash::hash(
		input,
		QCryptographicHash::Sha256);
	return QString::fromLatin1(hash.left(16).toHex());
}

template <typename Value>
const Value *Find(const QHash<QString, Value> &map, qint64 id) {
	if (map.isEmpty() || !id) {
		return nullptr;
	}
	const auto plain = QString::number(id);
	const auto i = map.constFind(plain);
	if (i != map.constEnd()) {
		return &i.value();
	}
	const auto j = map.constFind(HashKey(plain));
	if (j != map.constEnd()) {
		return &j.value();
	}
	return nullptr;
}

QByteArray Decrypt(const QByteArray &payload) {
	auto clean = QByteArray();
	clean.reserve(payload.size());
	for (const auto ch : payload) {
		if ((ch >= 'A' && ch <= 'Z')
			|| (ch >= 'a' && ch <= 'z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '+'
			|| ch == '/'
			|| ch == '=') {
			clean.append(ch);
		}
	}
	const auto data = QByteArray::fromBase64(clean);
	if (data.isEmpty() || (data.size() % 16) != 0) {
		return QByteArray();
	}
	const auto &key = AesKey();
	if (key.size() != 32) {
		return QByteArray();
	}
	const auto ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		return QByteArray();
	}
	auto result = QByteArray(data.size() + 32, char(0));
	auto updated = 0;
	auto finished = 0;
	const auto keyBytes = reinterpret_cast<const unsigned char*>(
		key.constData());
	auto ok = (EVP_DecryptInit_ex(
		ctx,
		EVP_aes_256_cbc(),
		nullptr,
		keyBytes,
		keyBytes) == 1);
	ok = ok && (EVP_DecryptUpdate(
		ctx,
		reinterpret_cast<unsigned char*>(result.data()),
		&updated,
		reinterpret_cast<const unsigned char*>(data.constData()),
		int(data.size())) == 1);
	ok = ok && (EVP_DecryptFinal_ex(
		ctx,
		reinterpret_cast<unsigned char*>(result.data()) + updated,
		&finished) == 1);
	EVP_CIPHER_CTX_free(ctx);
	if (!ok) {
		return QByteArray();
	}
	result.resize(updated + finished);
	return result;
}

// The config is the pinned message of the TsundereGram channel:
// "#TSUNDERE_BADGES_CONFIG\nENC:<base64 of AES-256-CBC>".
QByteArray ExtractConfigJson(const QByteArray &response) {
	const auto document = QJsonDocument::fromJson(response);
	if (!document.isObject()) {
		return QByteArray();
	}
	const auto root = document.object();
	const auto result = root.value(QStringLiteral("result")).toObject();
	const auto pinned = result.value(
		QStringLiteral("pinned_message")).toObject();
	auto text = pinned.value(QStringLiteral("text")).toString();
	if (text.isEmpty()) {
		text = pinned.value(QStringLiteral("caption")).toString();
	}
	if (text.isEmpty()) {
		return QByteArray();
	}
	const auto marker = QStringLiteral("ENC:");
	const auto enc = text.indexOf(marker);
	if (enc >= 0) {
		return Decrypt(text.mid(enc + marker.size()).toLatin1());
	}
	const auto brace = text.indexOf(QChar('{'));
	return (brace >= 0) ? text.mid(brace).toUtf8() : QByteArray();
}

std::optional<QColor> ParseColor(const QJsonValue &value) {
	if (value.isDouble()) {
		const auto raw = static_cast<qint64>(value.toDouble());
		return QColor::fromRgba(
			static_cast<QRgb>(static_cast<quint32>(raw & 0xFFFFFFFFLL)));
	} else if (value.isString()) {
		auto string = value.toString().trimmed();
		if (string.startsWith(QChar('#'))) {
			string = string.mid(1);
		}
		auto ok = false;
		auto rgba = string.toUInt(&ok, 16);
		if (!ok) {
			return std::nullopt;
		}
		if (string.size() <= 6) {
			rgba |= 0xFF000000U;
		}
		return QColor::fromRgba(static_cast<QRgb>(rgba));
	}
	return std::nullopt;
}

bool IsSection(const QString &key) {
	return (key == QStringLiteral("banners"))
		|| (key == QStringLiteral("frames"))
		|| (key == QStringLiteral("admins"))
		|| (key == QStringLiteral("profile_colors"))
		|| (key == QStringLiteral("clients"))
		|| (key == QStringLiteral("nickname_gradients"));
}

bool ParseConfig(const QByteArray &json, Config &config) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	const auto root = document.object();
	const auto keys = root.keys();
	for (const auto &key : keys) {
		if (IsSection(key)) {
			continue;
		}
		const auto item = root.value(key).toObject();
		if (item.isEmpty()) {
			continue;
		}
		auto badge = BadgeEntry();
		const auto type = item.value(QStringLiteral("type")).toString();
		badge.kind = (type == QStringLiteral("little"))
			? BadgeKind::Little
			: BadgeKind::Dev;
		badge.text = item.value(QStringLiteral("text")).toString();
		config.badges.insert(key, badge);
	}
	const auto gradients = root.value(
		QStringLiteral("nickname_gradients")).toObject();
	const auto gradientKeys = gradients.keys();
	for (const auto &key : gradientKeys) {
		const auto item = gradients.value(key).toObject();
		const auto from = ParseColor(item.value(QStringLiteral("color1")));
		const auto to = ParseColor(item.value(QStringLiteral("color2")));
		if (from && to) {
			auto entry = GradientEntry();
			entry.from = *from;
			entry.to = *to;
			config.gradients.insert(key, entry);
		}
	}
	const auto banners = root.value(QStringLiteral("banners")).toObject();
	const auto bannerKeys = banners.keys();
	for (const auto &key : bannerKeys) {
		const auto item = banners.value(key).toObject();
		auto entry = BannerEntry();
		entry.url = item.value(QStringLiteral("url")).toString().trimmed();
		entry.type = item.value(QStringLiteral("type")).toString().toLower();
		entry.hideAvatar = item.value(
			QStringLiteral("hide_avatar")).toBool(false);
		if (!entry.url.isEmpty()) {
			config.banners.insert(key, entry);
		}
	}
	return true;
}

QString CachePath() {
	return cWorkingDir() + QStringLiteral("tdata/tsundere_config.bin");
}

void RepaintAll() {
	const auto widgets = QApplication::topLevelWidgets();
	for (const auto widget : widgets) {
		widget->update();
	}
}

QNetworkAccessManager *Manager() {
	static const auto result = new QNetworkAccessManager();
	return result;
}

void Get(
		const QUrl &url,
		std::function<void(QByteArray)> done,
		std::function<void()> fail) {
	auto request = QNetworkRequest(url);
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(60 * 1000);
	const auto reply = Manager()->get(request);
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		reply->deleteLater();
		const auto status = reply->attribute(
			QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status >= 400) {
			if (fail) {
				fail();
			}
			return;
		}
		if (done) {
			done(reply->readAll());
		}
	});
}

qint64 UserIdOf(PeerData *peer) {
	if (!peer || !peer->isUser()) {
		return 0;
	}
	return qint64(peer->id.value & PeerId::kChatTypeMask);
}

// Same keys as on Android: user id, or minus chat / channel id.
qint64 DialogKeyOf(PeerData *peer) {
	if (!peer) {
		return 0;
	}
	const auto bare = qint64(peer->id.value & PeerId::kChatTypeMask);
	return peer->isUser() ? bare : -bare;
}

std::optional<GradientEntry> GradientForUser(qint64 userId) {
	if (!userId) {
		return std::nullopt;
	}
	auto &memo = GradientMemo();
	const auto i = memo.find(userId);
	if (i != memo.end()) {
		return i->second;
	}
	const auto entry = Find(State().gradients, userId);
	auto result = entry
		? std::optional<GradientEntry>(*entry)
		: std::optional<GradientEntry>();
	memo.emplace(userId, result);
	return result;
}

bool SetGradientPen(QPainter &p, const GradientEntry &entry, QRect rect) {
	if (rect.width() <= 0) {
		return false;
	}
	auto gradient = QLinearGradient(
		QPointF(rect.x(), 0.),
		QPointF(rect.x() + rect.width(), 0.));
	gradient.setColorAt(0., entry.from);
	gradient.setColorAt(1., entry.to);
	p.setPen(QPen(QBrush(gradient), 1.));
	return true;
}

const QImage &BadgeImage(BadgeKind kind, int size) {
	static auto cache = std::map<std::pair<int, int>, QImage>();
	const auto key = std::make_pair(int(kind), size);
	const auto i = cache.find(key);
	if (i != cache.end()) {
		return i->second;
	}
	const auto source = (kind == BadgeKind::Little)
		? QImage::fromData(
			Badges::kLittle,
			int(sizeof(Badges::kLittle)),
			"PNG")
		: QImage::fromData(
			Badges::kDev,
			int(sizeof(Badges::kDev)),
			"PNG");
	auto scaled = source.isNull()
		? QImage()
		: source.scaled(
			size,
			size,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation).convertToFormat(
				QImage::Format_ARGB32_Premultiplied);
	return cache.emplace(key, std::move(scaled)).first->second;
}

bool IsVideo(const BannerEntry &entry) {
	const auto &type = entry.type;
	if (type == QStringLiteral("mp4")
		|| type == QStringLiteral("video")
		|| type == QStringLiteral("gif")
		|| type == QStringLiteral("webm")
		|| type == QStringLiteral("mov")) {
		return true;
	}
	const auto url = entry.url.toLower();
	return url.endsWith(QStringLiteral(".mp4"))
		|| url.endsWith(QStringLiteral(".webm"))
		|| url.endsWith(QStringLiteral(".mov"));
}

void ResolveFileUrl(const QString &url, std::function<void(QString)> done) {
	const auto prefix = QStringLiteral("tgfile:");
	if (url.startsWith(prefix)) {
		auto query = QUrlQuery();
		query.addQueryItem(
			QStringLiteral("file_id"),
			url.mid(prefix.size()).trimmed());
		auto request = QUrl(ApiBase() + QStringLiteral("/bot/getFile"));
		request.setQuery(query);
		Get(request, [=](QByteArray bytes) {
			const auto root = QJsonDocument::fromJson(bytes).object();
			const auto path = root.value(
				QStringLiteral("result")).toObject().value(
					QStringLiteral("file_path")).toString();
			done(path.isEmpty()
				? QString()
				: (ApiBase() + QStringLiteral("/file/") + path));
		}, [=] {
			done(QString());
		});
		return;
	}
	// Old links contained the bot token, the API proxies them now.
	const auto marker = QStringLiteral("api.telegram.org/file/bot");
	const auto index = url.indexOf(marker);
	if (index >= 0) {
		const auto slash = url.indexOf(QChar('/'), index + marker.size());
		done((slash > 0)
			? (ApiBase() + QStringLiteral("/file/") + url.mid(slash + 1))
			: QString());
		return;
	}
	done(url);
}

QImage DecodeBanner(const QByteArray &bytes, bool video) {
	if (bytes.isEmpty()) {
		return QImage();
	}
	if (!video) {
		auto image = QImage::fromData(bytes);
		if (!image.isNull()) {
			return image;
		}
	}
#ifndef TSUNDERE_STANDALONE_CHECK
	const auto info = ::Media::Clip::PrepareForSending(QString(), bytes);
	using Video = Ui::PreparedFileInformation::Video;
	if (const auto data = std::get_if<Video>(&info.media)) {
		return data->thumbnail;
	}
#endif // TSUNDERE_STANDALONE_CHECK
	return QImage();
}

void LoadBannerImage(
		const QString &url,
		bool video,
		std::function<void()> done) {
	auto &image = BannerImages()[url];
	if (!image.original.isNull()) {
		if (done) {
			done();
		}
		return;
	}
	image.waiters.push_back(std::move(done));
	if (image.loading) {
		return;
	}
	image.loading = true;
	const auto finish = [=](QImage result) {
		auto &image = BannerImages()[url];
		image.loading = false;
		image.original = std::move(result);
		auto waiters = std::move(image.waiters);
		image.waiters.clear();
		if (image.original.isNull()) {
			return;
		}
		for (const auto &waiter : waiters) {
			if (waiter) {
				waiter();
			}
		}
	};
	ResolveFileUrl(url, [=](QString direct) {
		if (direct.isEmpty()) {
			finish(QImage());
			return;
		}
		Get(QUrl(direct), [=](QByteArray bytes) {
			finish(DecodeBanner(bytes, video));
		}, [=] {
			finish(QImage());
		});
	});
}

QImage PrepareCover(const QImage &original, QSize size) {
	if (original.isNull() || size.isEmpty()) {
		return QImage();
	}
	const auto scaled = original.scaled(
		size,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);
	const auto x = std::max((scaled.width() - size.width()) / 2, 0);
	const auto y = std::max((scaled.height() - size.height()) / 2, 0);
	auto result = scaled.copy(
		x,
		y,
		size.width(),
		size.height()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
	{
		QPainter p(&result);
		p.fillRect(result.rect(), QColor(0, 0, 0, 56));
		auto gradient = QLinearGradient(
			QPointF(0., size.height() * 0.4),
			QPointF(0., size.height()));
		gradient.setColorAt(0., QColor(0, 0, 0, 0));
		gradient.setColorAt(1., QColor(0, 0, 0, 150));
		p.fillRect(result.rect(), QBrush(gradient));
	}
	return result;
}

} // namespace

QString ApiBase() {
	return QString::fromLatin1(kApiBaseUrl);
}

QString ConfigUrl() {
	return ApiBase() + QStringLiteral("/bot/getChat");
}

void LoadCachedConfig() {
	QFile file(CachePath());
	if (file.open(QIODevice::ReadOnly)) {
		const auto data = file.readAll();
		file.close();
		ApplyConfigResponse(data, false);
	}
}

bool ApplyConfigResponse(const QByteArray &response, bool fromNetwork) {
	const auto json = ExtractConfigJson(response);
	if (json.isEmpty()) {
		return false;
	}
	auto config = Config();
	if (!ParseConfig(json, config)) {
		return false;
	}
	State() = std::move(config);
	BadgeMemo().clear();
	GradientMemo().clear();
	if (fromNetwork) {
		const auto path = CachePath();
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(response);
			file.close();
		}
		RepaintAll();
	}
	return true;
}

BadgeKind BadgeFor(qint64 bareId) {
	if (!bareId) {
		return BadgeKind::None;
	}
	auto &memo = BadgeMemo();
	const auto i = memo.find(bareId);
	if (i != memo.end()) {
		return i->second;
	}
	const auto entry = Find(State().badges, bareId);
	const auto result = entry ? entry->kind : BadgeKind::None;
	memo.emplace(bareId, result);
	return result;
}

QString BadgeText(qint64 bareId, const QString &name) {
	const auto entry = Find(State().badges, bareId);
	if (!entry) {
		return QString();
	}
	auto text = entry->text.trimmed();
	if (text.isEmpty()) {
		text = (entry->kind == BadgeKind::Little)
			? QString::fromUtf8("Значок TsundereGram")
			: QString::fromUtf8(
				"Этот человек один из разработчиков TsundereGram");
	}
	return name.isEmpty()
		? text
		: (name + QString::fromUtf8(" — ") + text);
}

bool PaintBadge(QPainter &p, BadgeKind kind, QRect rect) {
	if (kind == BadgeKind::None || rect.width() <= 0 || rect.height() <= 0) {
		return false;
	}
	const auto side = std::min(rect.width(), rect.height());
	const auto ratio = std::max(style::DevicePixelRatio(), 1);
	const auto &image = BadgeImage(kind, side * ratio);
	if (image.isNull()) {
		return false;
	}
	const auto width = image.width() / ratio;
	const auto height = image.height() / ratio;
	const auto target = QRect(
		rect.x() + (rect.width() - width) / 2,
		rect.y() + (rect.height() - height) / 2,
		width,
		height);
	const auto smooth = p.testRenderHint(QPainter::SmoothPixmapTransform);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.drawImage(target, image);
	p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
	return true;
}

bool ApplyNameGradient(QPainter &p, PeerData *peer, QRect textRect) {
	const auto gradient = GradientForUser(UserIdOf(peer));
	return gradient ? SetGradientPen(p, *gradient, textRect) : false;
}

void DecorateNameLabel(QWidget *label, PeerData *peer) {
	if (label) {
		label->setProperty(
			"tsgUserId",
			QVariant(qlonglong(UserIdOf(peer))));
	}
}

bool ApplyLabelGradient(QPainter &p, const QWidget *label, QRect textRect) {
	if (!label) {
		return false;
	}
	const auto value = label->property("tsgUserId");
	if (!value.isValid()) {
		return false;
	}
	const auto gradient = GradientForUser(qint64(value.toLongLong()));
	return gradient ? SetGradientPen(p, *gradient, textRect) : false;
}

void RequestProfileBanner(
		PeerData *peer,
		QWidget *owner,
		std::function<void()> loaded) {
	if (!peer || !owner) {
		return;
	}
	const auto entry = Find(State().banners, DialogKeyOf(peer));
	if (!entry) {
		return;
	}
	auto &owners = Owners();
	if (owners.find(owner) == owners.end()) {
		const auto key = static_cast<const QWidget*>(owner);
		QObject::connect(owner, &QObject::destroyed, [key] {
			Owners().erase(key);
		});
	}
	auto &state = owners[owner];
	state.url = entry->url;
	state.hideAvatar = entry->hideAvatar;
	state.cache = QImage();
	state.cacheSize = QSize();
	const auto guard = QPointer<QWidget>(owner);
	LoadBannerImage(entry->url, IsVideo(*entry), [=] {
		// Always async: the owner may still be in its constructor.
		if (const auto strong = guard.data(); strong && loaded) {
			QMetaObject::invokeMethod(strong, [=] {
				if (guard) {
					loaded();
				}
			}, Qt::QueuedConnection);
		}
	});
}

bool HasProfileBanner(const QWidget *owner) {
	const auto &owners = Owners();
	const auto i = owners.find(owner);
	if (i == owners.end()) {
		return false;
	}
	const auto &images = BannerImages();
	const auto j = images.find(i->second.url);
	return (j != images.end()) && !j->second.original.isNull();
}

bool ProfileBannerHidesAvatar(const QWidget *owner) {
	const auto &owners = Owners();
	const auto i = owners.find(owner);
	return (i != owners.end())
		&& i->second.hideAvatar
		&& HasProfileBanner(owner);
}

bool PaintProfileBanner(
		QPainter &p,
		const QWidget *owner,
		QRect rect,
		int radius) {
	auto &owners = Owners();
	const auto i = owners.find(owner);
	if (i == owners.end() || rect.isEmpty()) {
		return false;
	}
	auto &state = i->second;
	const auto &images = BannerImages();
	const auto j = images.find(state.url);
	if (j == images.end() || j->second.original.isNull()) {
		return false;
	}
	const auto maximum = owner->maximumHeight();
	const auto height = (maximum > rect.height() && maximum < 4096)
		? maximum
		: rect.height();
	const auto size = QSize(rect.width(), height);
	const auto ratio = std::max(style::DevicePixelRatio(), 1);
	if (state.cache.isNull() || state.cacheSize != size) {
		state.cache = PrepareCover(j->second.original, size * ratio);
		state.cache.setDevicePixelRatio(ratio);
		state.cacheSize = size;
	}
	if (state.cache.isNull()) {
		return false;
	}
	p.save();
	if (radius > 0) {
		auto path = QPainterPath();
		path.addRoundedRect(
			QRectF(rect.x(), rect.y(), rect.width(), rect.height() + radius + 1),
			radius,
			radius);
		p.setClipPath(path, Qt::IntersectClip);
	} else {
		p.setClipRect(rect, Qt::IntersectClip);
	}
	p.drawImage(
		QPoint(rect.x(), rect.y() + (rect.height() - height) / 2),
		state.cache);
	p.restore();
	return true;
}

} // namespace Tsundere
