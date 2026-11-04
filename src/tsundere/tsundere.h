// TsundereGram Desktop: profile features shared with TsundereGram for Android.
// Badges, nickname gradients and profile banners come from the same
// encrypted config the Android app reads (served by the TsundereGram API).
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QRect>
#include <QtCore/QString>

#include <functional>

class QPainter;
class QWidget;
class PeerData;

namespace Tsundere {

enum class BadgeKind {
	None,
	Dev,
	Little,
};

// Remote config.
[[nodiscard]] QString ApiBase();
[[nodiscard]] QString ConfigUrl();
void LoadCachedConfig();
bool ApplyConfigResponse(const QByteArray &response, bool fromNetwork);

// Badges (dev / little), looked up by a bare user id.
[[nodiscard]] BadgeKind BadgeFor(qint64 bareId);
[[nodiscard]] QString BadgeText(qint64 bareId, const QString &name);
bool PaintBadge(QPainter &p, BadgeKind kind, QRect rect);

// Nickname gradients: switch the painter pen to a horizontal gradient.
bool ApplyNameGradient(QPainter &p, PeerData *peer, QRect textRect);
void DecorateNameLabel(QWidget *label, PeerData *peer);
bool ApplyLabelGradient(QPainter &p, const QWidget *label, QRect textRect);

// Profile banners in the profile top bar.
void RequestProfileBanner(
	PeerData *peer,
	QWidget *owner,
	std::function<void()> loaded);
[[nodiscard]] bool HasProfileBanner(const QWidget *owner);
[[nodiscard]] bool ProfileBannerHidesAvatar(const QWidget *owner);
bool PaintProfileBanner(
	QPainter &p,
	const QWidget *owner,
	QRect rect,
	int radius);

} // namespace Tsundere
