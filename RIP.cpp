#include <queue>
#include <thread>
#include <mutex>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <pthread.h>
#include <FLAC++/all.h>
#include <QApplication>
#include <QMessageBox>
#include <QWidget>
#include <QMenu>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QProcess>
#include <QScriptEngine>
#include <QDebug>
#include <QSettings>
#include "Settings.h"
#include "ui_Info.h"
#include "RIP.h"
using namespace std;

template <class _T> string toString(_T const& _t)
{
	ostringstream o;
	o << _t;
	return o.str();
}

inline QString fSS(string const& _s)
{
	return QString::fromUtf8(_s.c_str());
}

inline string tSS(QString const& _s)
{
	return string(_s.toUtf8().data());
}

/* TODO: cover art
 * TODO: optimize for tag insertion
 */

static void paintLogo(QPainter& _p, QRectF _r, int _degrees = 360, QVector<QPair<float, float> > const& _split = QVector<QPair<float, float> >())
{
	(void)_degrees;
	float wx = _r.width();
	float wy = _r.height();

	_p.setRenderHint(QPainter::Antialiasing, true);

	_p.setPen(Qt::NoPen);
	_p.setBrush(QColor::fromHsv(0, 0, 224));
	float pwi = 0.07;
	float cp = 0.42;
	QRectF inner = _r.adjusted(wx * pwi, wy * pwi, -wx * pwi, -wy * pwi);

	_p.drawEllipse(_r);

	_p.setCompositionMode(QPainter::CompositionMode_Source);
	_p.setPen(Qt::NoPen);
	_p.setBrush(Qt::transparent);
	_p.drawEllipse(_r.adjusted(wx * cp, wy * cp, -wx * cp, -wy * cp));

	float lastPos = 0.f;
	for (QPair<float, float> f: _split)
	{
		if (f.first == 1.f)
			_p.setBrush(QColor::fromHsv(0, 0, 224));
		else
			_p.setBrush(QColor::fromHsv(0, 0, 112));
		_p.setPen(QPen(Qt::transparent, 1.f));
		_p.drawPie(inner, 90 * 16 - lastPos * 360 * 16, f.second * -360 * 16);
		if (f.first != 0.f && f.first != 1.f)
		{
			float pf = (cp - pwi) * (1.f - f.first);
			auto part = inner.adjusted(wx * pf, wy * pf, -wx * pf, -wy * pf);
			_p.setBrush(QColor::fromHsv(0, 32, 224));
			_p.setPen(Qt::NoPen);
			_p.drawPie(part, 90 * 16 - lastPos * 360 * 16, f.second * -360 * 16);
		}
		lastPos += f.second;
	}

	_p.setPen(Qt::NoPen);
	_p.setBrush(Qt::transparent);
	_p.drawEllipse(_r.adjusted(wx * cp, wy * cp, -wx * cp, -wy * cp));
}

static void paintComplexLogo(QPainter& _p, QRect _r, int _degrees = 360, QVector<QPair<float, float> > const& _split = QVector<QPair<float, float> >(), QColor _back = QColor::fromHsv(0, 128, 192), QColor _fore = QColor::fromHsv(0, 0, 232))
{
	int gs = _split.size() ? 100 : 105;
	float wx = _r.width();
	float wy = _r.height();
	QRect gr = _r.adjusted(wx * .3, wy * .3, -wx * .3, -wy * .3);
	QLinearGradient grad(gr.topLeft(), gr.bottomRight());
	_p.setRenderHint(QPainter::Antialiasing, true);
	_p.setPen(Qt::NoPen);
	grad.setStops(QGradientStops() << QGradientStop(0, _back) << QGradientStop(_split.size() ? 0.2 : 0.5, _back.lighter(gs)) << QGradientStop(_split.size() ? 0.21 : 0.51, _back.darker(gs)) << QGradientStop(1, _back));
	_p.setBrush(grad);
	_p.drawEllipse(_r);
	grad.setStops(QGradientStops() << QGradientStop(0, _fore) << QGradientStop(_split.size() ? 0.2 : 0.5, _fore.lighter(gs)) << QGradientStop(_split.size() ? 0.21 : 0.51, _fore.darker(gs)) << QGradientStop(1, _fore));
	_p.setBrush(grad);
	if (_degrees)
		_p.drawPie(_r, 90 * 16, -_degrees * 16);
	_p.setPen(QColor::fromHsv(0, 0, 0, 96));
	_p.setBrush(Qt::NoBrush);
	_p.drawEllipse(_r);

	_p.setPen(QPen(QColor::fromHsv(0, 0, 0, 32), 0, Qt::DotLine));
	_p.setBrush(QColor::fromHsv(0, 0, 0, 8));
	float lastPos = 0.f;
	for (int i = 1; i < _split.size(); i += 2)
		_p.drawPie(_r, 90 * 16 - (lastPos += _split[i].second + _split[i - 1].second) * 360 * 16, _split[i].second * 360 * 16);

	_p.setCompositionMode(QPainter::CompositionMode_Source);
	_p.setPen(Qt::NoPen);
	_p.setBrush(Qt::transparent);
	_p.drawEllipse(_r.adjusted(wx * .4, wy * .4, -wx * .4, -wy * .4));
	_p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	_p.setPen(QColor::fromHsv(0, 0, 0, 96));
	_p.setBrush(Qt::NoBrush);
	_p.drawEllipse(_r.adjusted(wx * .4, wy * .4, -wx * .4, -wy * .4));
}

Progress::Progress(RIP* _r): QWidget(0, Qt::WindowStaysOnTopHint|Qt::FramelessWindowHint), m_r(_r)
{
	QApplication::setQuitOnLastWindowClosed(false);
	resize(156, 192);
	setWindowIcon(QIcon(":/RipInPeace.png"));
}

void Progress::paintEvent(QPaintEvent*)
{
	auto f = m_r->progressVector();

	QPainter p(this);
	auto r = rect();
	QPixmap px(140, 140);
	int m = 6;
	px.fill(Qt::transparent);
	{
		QPainter p(&px);
		paintLogo(p, QRect(m, m, px.width() - m * 2, px.height() - m * 2), 360 * m_r->amountDone(), f);
	}
	QLinearGradient g(r.topLeft(), r.bottomLeft());
	g.setStops(QGradientStops() << QGradientStop(0, QColor::fromHsv(0, 0, 96)) << QGradientStop(1, QColor::fromHsv(0, 0, 48)));
	p.fillRect(r, g);
	p.drawPixmap((width() - px.width()) / 2, 0, px);
	p.setPen(QColor::fromHsv(0, 0, 64));
	p.drawRect(r.adjusted(0, 0, -1, -1));
	p.setPen(Qt::black);
	p.drawText(QRect(m, px.height() - m - 1, width() - m * 2, height() - px.height() + m), Qt::AlignCenter|Qt::TextWordWrap, m_r->toolTip());
	p.setPen(QColor::fromHsv(0, 0, 248));
	p.drawText(QRect(m, px.height() - m, width() - m * 2, height() - px.height() + m), Qt::AlignCenter|Qt::TextWordWrap, m_r->toolTip());
}

RIP::RIP():
	m_path("/Music"),
	m_filename("(albumartist ? albumartist : 'Various Artists') + ' - ' +\n album + (disctotal > 1 ? ' ['+discnumber+'-'+disctotal+']' : '') +\n '/' + sortnumber + ' ' + (compilation ? artist+' - ' : '') +\n title + '.flac'"),
	m_device("/dev/cdrom"),
	m_paranoia(Paranoia::defaultFlags()),
	m_squeeze(0),
	m_ripper(nullptr),
	m_identifier(nullptr),
	m_ripped(false),
	m_identified(false),
	m_confirmed(false),
	m_lastPercentDone(0)
{
	{
		QPixmap px(22, 22);
		px.fill(Qt::transparent);
		{
			QPainter p(&px);
			p.setOpacity(0.95);
			paintLogo(p, px.rect().adjusted(1, 1, -1, -1));
		}
		m_inactive = QIcon(px);
	}

	setIcon(m_inactive);

	readSettings();

	m_progressPie = new Progress(this);
	m_settings = new Settings(m_progressPie, this);
	createPopup();
	m_popup->setEnabled(false);

	connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(onActivated(QSystemTrayIcon::ActivationReason)));
	setContextMenu(new QMenu("Rip in Peace"));
	(m_abortRip = contextMenu()->addAction("Abort Rip", this, SLOT(onAbortRip())))->setEnabled(false);
	(m_unconfirm = contextMenu()->addAction("Unconfirm", this, SLOT(onUnconfirm())))->setEnabled(false);
#if !defined(FINAL)
	(m_testIcon = contextMenu()->addAction("Test Icon"))->setCheckable(true);
#endif
	contextMenu()->addSeparator();
	contextMenu()->addAction("Settings", m_settings, SLOT(show()));
	contextMenu()->addAction("About", this, SLOT(onAbout()));
	contextMenu()->addAction("Quit", this, SLOT(onQuit()));

//	connect(this, SIGNAL(messageClicked()), m_popup, SLOT(show()));

	startTimer(1000);
}

RIP::~RIP()
{
	writeSettings();
	m_aborting = QTime::currentTime();
	if (m_identifier)
	{
		m_identifier->join();
		delete m_identifier;
		m_identifier = nullptr;
	}
	if (m_ripper)
	{
		pthread_cancel(m_ripper->native_handle());
		m_ripper->join();
		delete m_ripper;
		m_ripper = nullptr;
	}
}

QVector<QPair<float, float> > RIP::progressVector() const
{
	size_t total = 0;
	QVector<QPair<float, float> > ret;
	ret.reserve(m_progress.size());
	for (auto i: m_progress)
		total += i.second;
	for (auto i: m_progress)
		ret.push_back(QPair<float, float>(float(i.first) / i.second, float(i.second) / total));
	return ret;
}

void RIP::createPopup()
{
	m_popup = new QFrame(m_progressPie, Qt::WindowStaysOnTopHint|Qt::FramelessWindowHint|Qt::Window);
	m_info.setupUi(m_popup);
	for (DiscInfo const& di: m_dis)
		m_info.presets->addItem(fSS(di.artist + " - " + di.title));
	connect(m_info.confirm, SIGNAL(clicked()), SLOT(onConfirm()));
	connect(m_info.presets, SIGNAL(currentIndexChanged(int)), SLOT(updatePreset(int)));
}

void RIP::readSettings()
{
	QSettings s("Gav", "RIP");
	m_filename = tSS(s.value("filename", fSS(m_filename)).toString());
	m_path = tSS(s.value("directory", fSS(m_path)).toString());
	m_device = tSS(s.value("device", fSS(m_device)).toString());
	m_paranoia = s.value("paranoia", m_paranoia).toInt();
	m_squeeze = s.value("squeeze", m_squeeze).toInt();
}

void RIP::writeSettings()
{
	QSettings s("Gav", "RIP");
	s.setValue("filename", fSS(m_filename));
	s.setValue("directory", fSS(m_path));
	s.setValue("device", fSS(m_device));
	s.setValue("paranoia", m_paranoia);
	s.setValue("squeeze", m_squeeze);
}

void RIP::updatePreset(int _i)
{
	m_di = (int)m_dis.size() > _i && _i >= 0 ? m_dis[_i] : DiscInfo(m_p.tracks());
	update();
	plantInfo();
}

void RIP::plantInfo()
{
	m_info.title->setText(fSS(m_di.title));
	m_info.artist->setText(fSS(m_di.artist));
	m_info.setIndex->setValue(m_di.setIndex + 1);
	m_info.setTotal->setValue(m_di.setTotal);
	m_info.year->setValue(m_di.year);
	m_info.tracks->setRowCount(m_p.tracks());
	for (unsigned i = 0; i < m_di.tracks.size(); ++i)
	{
		m_info.tracks->setItem(i, 0, new QTableWidgetItem(fSS(m_di.tracks[i].title)));
		m_info.tracks->setItem(i, 1, new QTableWidgetItem(fSS(m_di.tracks[i].artist)));
		if (m_p.trackLength(i) == 0)
			m_info.tracks->hideRow(i);
	}
}

void RIP::harvestInfo()
{
	m_di.title = tSS(m_info.title->text());
	m_di.artist = tSS(m_info.artist->text());
	m_di.setIndex = m_info.setIndex->value() - 1;
	m_di.setTotal = m_info.setTotal->value();
	m_di.year = m_info.year->value();
	for (unsigned i = 0; i < m_di.tracks.size(); ++i)
	{
		m_di.tracks[i].title = tSS(m_info.tracks->item(i, 0)->text());
		m_di.tracks[i].artist = tSS(m_info.tracks->item(i, 1)->text());
	}
}

void RIP::onAbout()
{
	QMessageBox::about(m_progressPie, "About Rip in Peace", "<b>Rip in Peace</b><br/>v1.0.0<p>A Ripper that doesn't get in your way.<p>By <b>Gav Wood</b>, 2012.");
}

void RIP::onAbortRip()
{
	m_aborting = QTime::currentTime();
}

void RIP::onQuit()
{
	qApp->quit();
}

void RIP::onActivated(QSystemTrayIcon::ActivationReason _r)
{
	// Ignore events if we're spinning-up the CD drive - it doesn't do us any good, and yet it seems to mean windows cannot be displayed?
	if (_r == QSystemTrayIcon::Trigger && m_ripper)
	{
		if (m_confirmed && m_progressPie)
			if (m_progressPie->isVisible())
				m_progressPie->hide();
			else
			{
				m_progressPie->show();
				m_progressPie->show();
			}
		else
			if (m_popup->isVisible())
				m_popup->hide();
			else
				showPopup();
	}
}

void RIP::showPopup()
{
	harvestInfo();
	delete m_popup;
	createPopup();
	m_popup->move(geometry().topLeft());
	plantInfo();
	m_popup->show();
}

void RIP::onConfirm()
{
	m_confirmed = true;
	update();
	m_unconfirm->setEnabled(true);
	if (m_popup->isVisible())
		m_popup->hide();
}

void RIP::onUnconfirm()
{
	m_confirmed = false;
	update();
	m_unconfirm->setEnabled(false);
	m_progressPie->hide();
	showPopup();
}

void RIP::tagAll()
{
	for (unsigned i = 0; i < m_di.tracks.size(); ++i)
		if (m_p.trackLength(i))
		{
			string filename = m_path + "/" + m_temp + "/" + toString(i);

			FLAC::Metadata::Chain chain;
			if (!chain.read(filename.c_str()))
			{
				cerr << "ERROR: Couldn't read FLAC chain." << endl;
				continue;
			}

			FLAC::Metadata::VorbisComment* vc = nullptr;
			{
				FLAC::Metadata::Iterator iterator;
				iterator.init(chain);
				do {
					FLAC::Metadata::Prototype* block = iterator.get_block();
					if (block->get_type() == FLAC__METADATA_TYPE_VORBIS_COMMENT)
					{
						vc = (FLAC::Metadata::VorbisComment*)block;
						break;
					}
				} while (iterator.next());
				if (!vc)
				{
					while (iterator.next()) {}
					vc = new FLAC::Metadata::VorbisComment();
					vc->set_is_last(true);
					if (!iterator.insert_block_after(vc))
					{
						delete vc;
						cerr << "ERROR: Couldn't insert vorbis comment in chain." << endl;
						continue;
					}
				}
				while (vc->get_num_comments() > 0)
					vc->delete_comment(vc->get_num_comments() - 1);
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("album", m_di.title.c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("albumartist", m_di.artist.c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("discid", m_di.discid.c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("date", toString(m_di.year).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("tracknumber", toString(i + 1).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("title", m_di.tracks[i].title.c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("artist", (m_di.tracks[i].artist.size() ? m_di.tracks[i].artist : m_di.artist).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("discnumber", toString(m_di.setIndex + 1).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("disctotal", toString(m_di.setTotal).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("tracktotal", toString(m_di.tracks.size()).c_str()));
				vc->append_comment(FLAC::Metadata::VorbisComment::Entry("totaltracks", toString(m_di.tracks.size()).c_str()));
			}

			if (!chain.write())
			{
				cerr << "ERROR: Couldn't write FLAC chain." << endl;
				continue;
			}

			delete vc;
		}
}

QString scrubbed(QString _s)
{
	_s.replace('*', "");
	_s.replace(':', "");
	_s.replace('?', "");
	_s.replace('/', "");
	_s.replace('\\', "");
	return _s;
}

void RIP::moveAll()
{
	QScriptEngine s;
	// RIP-style variables - deprecated.
	s.globalObject().setProperty("disctitle", scrubbed(fSS(m_di.title)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("discartist", scrubbed(fSS(m_di.artist)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("index", m_di.setIndex + 1, QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("total", m_di.setTotal, QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("year", m_di.year == 1900 ? QString() : QString::number(m_di.year), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	// Vorbiscomment-style variables. Use these.
	s.globalObject().setProperty("compilation", m_di.isCompilation(), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("album", scrubbed(fSS(m_di.title)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("albumartist", scrubbed(fSS(m_di.artist)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("discnumber", m_di.setIndex + 1, QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("disctotal", m_di.setTotal, QScriptValue::ReadOnly|QScriptValue::Undeletable);
	s.globalObject().setProperty("date", m_di.year == 1900 ? QString() : QString::number(m_di.year), QScriptValue::ReadOnly|QScriptValue::Undeletable);
	for (unsigned i = 0; i < m_di.tracks.size(); ++i)
		if (m_p.trackLength(i))
		{
			// RIP-style variables - deprecated.
			s.globalObject().setProperty("number", i + 1, QScriptValue::ReadOnly|QScriptValue::Undeletable);
			// Vorbiscomment-style variables. Use these.
			s.globalObject().setProperty("tracknumber", i + 1, QScriptValue::ReadOnly|QScriptValue::Undeletable);
			s.globalObject().setProperty("sortnumber", QString("%1").arg(i + 1, 2, 10, QChar('0')), QScriptValue::ReadOnly|QScriptValue::Undeletable);
			s.globalObject().setProperty("title", scrubbed(fSS(m_di.tracks[i].title)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
			s.globalObject().setProperty("artist", scrubbed(fSS(m_di.tracks[i].artist)), QScriptValue::ReadOnly|QScriptValue::Undeletable);
			auto filename = s.evaluate(fSS(m_filename)).toString();
			QDir().mkpath((fSS(m_path) + "/" + filename).section('/', 0, -2));
			QFile::rename(fSS(m_path + "/" + m_temp) + QString("/%1").arg(i), fSS(m_path) + "/" + filename);
		}
	QDir().rmdir(fSS(m_path + "/" + m_temp));
}

float RIP::amountDone() const
{
	size_t total = 0;
	size_t done = 0;
	for (auto p: m_progress)
	{
		done += p.first;
		total += p.second;
	}
	return total ? float(done) / total : -1.f;
}

void RIP::update()
{
	float ad = amountDone();
	QPixmap px(22, 22);
	px.fill(Qt::transparent);
	{
		auto pv = progressVector();
		QString m = m_confirmed ? "" : m_info.title->text().isEmpty() ? "!" : "?";
#if !defined(FINAL)
		if (m_testIcon->isChecked())
		{
			pv.clear();
			pv += QPair<float, float>(1, .2);
			pv += QPair<float, float>(1, .1);
			pv += QPair<float, float>(1, .15);
			pv += QPair<float, float>(m_lastPercentDone / 100.f, .25);
			pv += QPair<float, float>(0, .3);
			m = /*ad < .3f ? "!" : ad < .6f ? "?" :*/ "";
		}
#endif
		QPainter p(&px);
		paintLogo(p, px.rect().adjusted(1, 1, -1, -1), ad * 360, pv);
		if (m.size())
		{
			p.setPen(QColor(64, 64, 64));
			p.drawText(px.rect().translated(1, 0), Qt::AlignCenter, m);
			p.drawText(px.rect().translated(-1, 0), Qt::AlignCenter, m);
			p.drawText(px.rect().translated(0, 1), Qt::AlignCenter, m);
			p.drawText(px.rect().translated(0, -1), Qt::AlignCenter, m);
			p.setPen(QColor(192, 192, 192));
			p.drawText(px.rect(), Qt::AlignCenter, m);
		}
	}
	setIcon(QIcon(px));
}

void RIP::timerEvent(QTimerEvent*)
{
	m_popup->move(geometry().topLeft());
	m_progressPie->move(geometry().topLeft());
	if (m_progressPie->isVisible())
		m_progressPie->update();
	if (m_aborting.isValid() && m_aborting.elapsed() > 1000)
	{
		pthread_cancel(m_ripper->native_handle());
		m_ripped = true;
	}

	if (m_ripped)
	{
		if ((m_identified && m_started.elapsed() > 30000 && m_justRipped && !m_popup->isVisible()) || m_aborting.isValid() || m_confirmed)
		{
			m_ripper->join();
			m_identifier->join();
			delete m_ripper;
			m_ripper = nullptr;
			delete m_identifier;
			m_identifier = nullptr;
			m_ripped = false;
			m_popup->hide();
			m_progressPie->hide();
			if (m_aborting.isNull())
			{
				harvestInfo();
				if (m_dis.size() || !m_di.title.empty())
				{
					tagAll();
					moveAll();
				}
				else
					showMessage("Unknown CD", "Couldn't find the CD with the available resources. Please reinsert once a database entry is accessible and the rip will be finished.");
			}
			m_p.close();
			m_progress.clear();
			eject();
			m_dis.clear();
			m_info.presets->clear();
			updatePreset(-1);
			m_popup->setEnabled(false);
			m_aborting = QTime();
			m_abortRip->setEnabled(false);
			m_unconfirm->setEnabled(false);
			m_progressPie->hide();
			m_justRipped = false;
		}
		else if (m_justRipped)
		{
			if (m_dis.size() || !m_di.title.empty())
			{
				showMessage("Please confirm", "Please click to inspect and confirm the CD details entered or abort the rip operation now and re-enter the CD to continue later.");
				m_justRipped = false;
			}
			else if (!m_popup->isVisible() && m_identified)
			{
				showMessage("Unknown CD", "Couldn't find the CD with the available resources. Please click to specify and confirm the correct artist/title information or abort the rip.");
				m_justRipped = false;
			}
			else if (m_popup->isVisible())
			{
				showMessage("Data acquired", "All audio data from the CD is ripped; press confirm once the CD information is complete.");
				m_justRipped = false;
			}
		}
	}
	if (!m_ripper)
	{
		if (m_p.open(m_device) && m_p.tracks() > 0)
		{
			m_startingRip = true;
			// Begin ripping.
			m_lastPercentDone = 100;
			m_progress.clear();
			for (unsigned i = 0; i < m_p.tracks(); ++i)
				m_progress.push_back(make_pair(0, m_p.trackLength(i)));

			if (m_id.identify(m_device))
			{
				m_temp = "RIP-" + m_id.asString();
				QDir().mkpath(fSS(m_path + "/" + m_temp));
				m_confirmed = false;
				m_aborting = QTime();
				m_identified = false;
				m_started.restart();
				m_popup->setEnabled(true);
				m_ripper = new std::thread([&](){ rip(); m_ripped = m_justRipped = true; });
				m_abortRip->setEnabled(true);
				m_identifier = new std::thread([&](){ getDiscInfo(); m_identified = true; });
			}
		}
		else
		{
			m_p.close();
			setIcon(m_inactive);
		}
	}
	if (m_ripper && m_aborting.isNull() && m_identified && !m_confirmed && m_info.title->text().isEmpty() && !m_dis.size())
	{
		m_identified = false;
		if (m_identifier)
		{
			m_identifier->join();
			delete m_identifier;
		}
		m_identifier = new std::thread([&](){ getDiscInfo(); m_identified = true; });
	}
	if (m_identified && m_info.title->text().isEmpty() && m_dis.size())
	{
		m_info.presets->clear();
		for (DiscInfo const& di: m_dis)
			m_info.presets->addItem(fSS(di.artist + " - " + di.title));
		updatePreset(0);
	}

	QString tt;
	if (m_progress.size())
		for (unsigned i = 0; i < m_progress.size(); ++i)
			if (m_progress[i].first != 0 && m_progress[i].first != m_progress[i].second && m_info.tracks->item(i, 0))
				tt += QString("%1: %2%\n").arg((int)i < m_info.tracks->rowCount() && m_info.tracks->item(i, 0)->text().size() ? m_info.tracks->item(i, 0)->text() : QString("Track %1").arg(i + 1)).arg(int(m_progress[i].first * 100.0 / m_progress[i].second));
			else{}
	else
		tt = "Ready\n";
	tt.chop(1);
	setToolTip(tt);

	int percDone = amountDone() * 100;
#if !defined(FINAL)
	if (m_testIcon->isChecked())
		percDone = m_lastPercentDone == 100 ? 0 : (m_lastPercentDone + 5);
#endif
	if (percDone >= 0)
	{
		update();
		if (percDone >= 90 && m_lastPercentDone < 90 && !m_confirmed && !m_info.title->text().isEmpty())
			showMessage("Ripping nearly finished", "Ripping is almost complete; tagging will begin shortly. Are you sure the tags are OK?");
		m_lastPercentDone = percDone;
	}
}

void RIP::eject()
{
	QProcess::execute("eject", QStringList() << fSS(m_device));
}

void RIP::getDiscInfo()
{
	m_dis = m_id.lookup(m_p.tracks(), [&](){return m_aborting.isValid(); });
}

size_t flacLength(string const& _fn)
{
	FLAC::Metadata::StreamInfo si;
	if (FLAC::Metadata::get_streaminfo(_fn.c_str(), si))
		return si.get_total_samples();
	return 0;
}

void RIP::rip()
{
	unsigned t = m_p.tracks();
	vector<std::thread*> encoders;
	for (unsigned i = 0; i < t && m_aborting.isNull(); ++i)
		if (m_progress[i].second)
		{
			string fn = ((ostringstream&)(ostringstream()<<m_path<<"/"<<m_temp<<"/"<<i)).str();
			if (QFile::exists(fSS(fn)) && flacLength(fn) == m_progress[i].second)
			{
				m_progress[i].first = m_progress[i].second;
				continue;
			}
			auto m = new mutex;
			auto incoming = new queue<int32_t*>;
			auto encoder = [=]()
			{
				this->m_startingRip = false;
				FLAC::Encoder::File f;
				f.init(fn);
				f.set_channels(2);
				f.set_bits_per_sample(16);
				f.set_sample_rate(44100);
				f.set_compression_level(m_squeeze);
				f.set_total_samples_estimate(m_progress[i].second);

				while (true)
				{
					m->lock();
					if (incoming->size())
					{
						auto n = incoming->front();
						if (!n)
							break;
						incoming->pop();
						m->unlock();
						f.process_interleaved(n, Paranoia::frameLength());
						delete n;
						m_progress[i].first += Paranoia::frameLength();
					}
					else
					{
						m->unlock();
						usleep(100000);
					}
				}
				f.finish();
				delete incoming;
				delete m;
			};
			auto ripper = [&](unsigned, unsigned, int16_t const* d) -> bool
			{
				int32_t* o = new int32_t[Paranoia::frameLength() * 2];
				auto dl = d + Paranoia::frameLength() * 2;
				for (int32_t* oi = o; d < dl; d += 8, oi += 8)
					oi[0] = d[0], oi[1] = d[1], oi[2] = d[2], oi[3] = d[3],
					oi[4] = d[4], oi[5] = d[5], oi[6] = d[6], oi[7] = d[7];
				m->lock();
				incoming->push(o);
				m->unlock();
				return m_aborting.isNull();
			};
			encoders.push_back(new std::thread(encoder));
			m_p.rip(i, ripper);
			m->lock();
			incoming->push(nullptr);
			m->unlock();
		}
	for (auto e: encoders)
	{
		e->join();
		delete e;
	}
	encoders.clear();
}


