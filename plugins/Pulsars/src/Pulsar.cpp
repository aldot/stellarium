/*
 * Copyright (C) 2012 Alexander Wolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "Pulsar.hpp"
#include "Pulsars.hpp"
#include "StelObject.hpp"
#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelTexture.hpp"
#include "StelUtils.hpp"
#include "StelTranslator.hpp"
#include "StelModuleMgr.hpp"
#include "StelSkyDrawer.hpp"
#include "StelProjector.hpp"

#include <QTextStream>
#include <QDebug>
#include <QVariant>
#include <QVariantMap>
#include <QVariant>
#include <QList>

#define PSR_INERTIA 1.0e45 /* Typical moment of inertia for a pulsar */

StelTextureSP Pulsar::markerTexture;

bool Pulsar::distributionMode = false;
bool Pulsar::glitchFlag = false;
Vec3f Pulsar::markerColor = Vec3f(0.4f,0.5f,1.0f);
Vec3f Pulsar::glitchColor = Vec3f(0.2f,0.3f,1.0f);

Pulsar::Pulsar(const QVariantMap& map)
	: initialized(false)
	, designation("")
	, RA(0.)
	, DE(0.)
	, parallax(0.)
	, period(0.)
	, frequency(0.)
	, pfrequency(0.)
	, pderivative(0.)
	, dmeasure(0.)
	, bperiod(0.)
	, eccentricity(0.)
	, w50(0.)
	, s400(0.)
	, s600(0.)
	, s1400(0.)
	, distance(0.)
	, glitch(-1)
	, notes("")
{
	if (!map.contains("designation") || !map.contains("RA") || !map.contains("DE"))
	{
		qWarning() << "Pulsar: INVALID pulsar!" << map.value("designation").toString();
		qWarning() << "Pulsar: Please, check your 'pulsars.json' catalog!";
		return;
	}

	designation  = map.value("designation").toString();
	parallax = map.value("parallax").toFloat();
	period = map.value("period").toDouble();
	bperiod = map.value("bperiod").toDouble();
	frequency = map.value("frequency").toDouble();
	pfrequency = map.value("pfrequency").toDouble();
	pderivative = map.value("pderivative").toDouble();
	dmeasure = map.value("dmeasure").toDouble();
	eccentricity = map.value("eccentricity").toDouble();
	RA = StelUtils::getDecAngle(map.value("RA").toString());
	DE = StelUtils::getDecAngle(map.value("DE").toString());
	w50 = map.value("w50").toFloat();
	s400 = map.value("s400").toFloat();
	s600 = map.value("s600").toFloat();
	s1400 = map.value("s1400").toFloat();
	distance = map.value("distance").toFloat();
	glitch = map.value("glitch").toInt();
	notes = map.value("notes").toString();

	// If barycentric period not set then calculate it
	if (period==0 && frequency>0)
	{
		period = 1/frequency;
	}
	// If barycentric period derivative not set then calculate it
	if (pderivative==0)
	{
		pderivative = getP1(period, pfrequency);
	}

	initialized = true;
}

Pulsar::~Pulsar()
{
	//
}

QVariantMap Pulsar::getMap(void)
{
	QVariantMap map;
	map["designation"] = designation;
	map["parallax"] = parallax;
	map["bperiod"] = bperiod;
	map["frequency"] = frequency;
	map["pfrequency"] = pfrequency;
	map["pderivative"] = pderivative;
	map["dmeasure"] = dmeasure;
	map["eccentricity"] = eccentricity;	
	map["RA"] = RA;
	map["DE"] = DE;
	map["period"] = period;	
	map["w50"] = w50;
	map["s400"] = s400;
	map["s600"] = s600;
	map["s1400"] = s1400;
	map["distance"] = distance;
	map["glitch"] = glitch;
	map["notes"] = notes;

	return map;
}

float Pulsar::getSelectPriority(const StelCore* core) const
{
	return StelObject::getSelectPriority(core)-2.f;
}

QString Pulsar::getInfoString(const StelCore* core, const InfoStringGroup& flags) const
{
	QString str;
	QTextStream oss(&str);

	if (flags&Name)
	{
		oss << "<h2>" << designation << "</h2>";
	}

	if (flags&ObjectType)
	{

		if (glitch==0)
			oss << q_("Type: <b>%1</b>").arg(q_("pulsar")) << "<br />";
		else
		{
			QString sglitch;
			if (glitch==1)
				sglitch = q_("has one registered glitch");
			else
			{
				// TRANSLATORS: Full phrase is "Has X registered glitches", where X is number
				sglitch = q_("has %1 registered glitches").arg(glitch);
			}
			oss << q_("Type: <b>%1</b> (%2)").arg(q_("pulsar with glitches")).arg(sglitch) << "<br />";
		}
	}

	// Ra/Dec etc.
	oss << getPositionInfoString(core, flags);

	if (flags&Extra)
	{
		if (period>0)
		{
			//TRANSLATORS: Unit of measure for period - seconds
			oss << q_("Barycentric period: %1 s").arg(QString::number(period, 'f', 16)) << "<br>";
		}
		if (pderivative>0)
		{
			oss << q_("Time derivative of barycentric period: %1").arg(QString::number(pderivative, 'e', 5)) << "<br>";
		}
		if (dmeasure>0)
		{
			oss << QString("%1 %2 %3<sup>-3</sup> %4 %5")
			       .arg(q_("Dispersion measure:"))
			       .arg(QString::number(dmeasure, 'f', 3))
			       //TRANSLATORS: Unit of measure for distance - centimeters
			       .arg(q_("cm"))
			       .arg(QChar(0x00B7))
			       //TRANSLATORS: Unit of measure for distance - parsecs
			       .arg(q_("pc"));
			oss << "<br>";
		}
		double edot = getEdot(period, pderivative);
		if (edot>0)
		{
			oss << q_("Spin down energy loss rate: %1 ergs/s").arg(QString::number(edot, 'e', 2)) << "<br>";
		}
		if (bperiod>0)
		{
			oss << q_("Binary period of pulsar: %1 days").arg(QString::number(bperiod, 'f', 12)) << "<br>";
		}
		if (eccentricity>0)
		{
			oss << q_("Eccentricity: %1").arg(QString::number(eccentricity, 'f', 10)) << "<br>";
		}
		if (parallax>0)
		{
			//TRANSLATORS: Unit of measure for annual parallax - milliarcseconds
			oss << q_("Annual parallax: %1 mas").arg(parallax) << "<br>";
		}
		if (distance>0)
		{
			oss << q_("Distance based on electron density model: %1 kpc (%2 ly)").arg(distance).arg(distance*3261.563777) << "<br>";
		}
		if (w50>0)
		{
			oss << q_("Profile width at 50% of peak: %1 ms").arg(QString::number(w50, 'f', 2)) << "<br>";
		}
		if (s400>0)
		{
			oss << QString("%1 %2%3: %4 %5")
			       // TRANSLATORS: Full phrase is "Time averaged flux density at XXXMHz"
			       .arg(q_("Time averaged flux density at"))
			       .arg(400)
			       // TRANSLATORS: Unit of measurement of frequency
			       .arg(q_("MHz"))
			       .arg(QString::number(s400, 'f', 2))
			       // TRANSLATORS: mJy is milliJansky(10-26W/m2/Hz)
			       .arg(q_("mJy")) << "<br>";
		}
		if (s600>0)
		{
			oss << QString("%1 %2%3: %4 %5")
			       // TRANSLATORS: Full phrase is "Time averaged flux density at XXXMHz"
			       .arg(q_("Time averaged flux density at"))
			       .arg(600)
			       // TRANSLATORS: Unit of measurement of frequency
			       .arg(q_("MHz"))
			       .arg(QString::number(s600, 'f', 2))
			       // TRANSLATORS: mJy is milliJansky(10-26W/m2/Hz)
			       .arg(q_("mJy")) << "<br>";
		}
		if (s1400>0)
		{
			oss << QString("%1 %2%3: %4 %5")
			       // TRANSLATORS: Full phrase is "Time averaged flux density at XXXMHz"
			       .arg(q_("Time averaged flux density at"))
			       .arg(1400)
			       // TRANSLATORS: Unit of measurement of frequency
			       .arg(q_("MHz"))
			       .arg(QString::number(s1400, 'f', 2))
			       // TRANSLATORS: mJy is milliJansky(10-26W/m2/Hz)
			       .arg(q_("mJy")) << "<br>";
		}		
		if (notes.length()>0)
		{
			oss << "<br>" << q_("Notes: %1").arg(getPulsarTypeInfoString(notes)) << "<br>";
		}
	}

	postProcessInfoString(str, flags);
	return str;
}

Vec3f Pulsar::getInfoColor(void) const
{
	return Vec3f(1.0, 1.0, 1.0);
}

float Pulsar::getVMagnitude(const StelCore* core) const
{
	Q_UNUSED(core);
	// Calculate fake visual magnitude as function by distance - minimal magnitude is 6
	float vmag = distance + 6.f;

	if (distributionMode)
	{
		return 3.f;
	}
	else
	{
		return vmag;
	}
}

float Pulsar::getVMagnitudeWithExtinction(const StelCore *core) const
{
	return getVMagnitude(core);
}

double Pulsar::getEdot(double p0, double p1) const
{
	if (p0>0 && p1!=0)
	{
		// Calculate spin down energy loss rate (ergs/s)
		return 4.0 * M_PI * M_PI * PSR_INERTIA * p1 / pow(p0,3);
	}
	else
	{
		return 0.0;
	}
}

double Pulsar::getP1(double p0, double f1) const
{
	if (p0>0 && f1!=0)
	{
		// Calculate derivative of barycentric period
		return -1.0 * p0 * p0 * f1;
	}
	else
	{
		return 0.0;
	}
}


QString Pulsar::getPulsarTypeInfoString(QString pcode) const
{
	QStringList out;

	if (pcode.contains("AXP"))
	{
		out.append(q_("anomalous X-ray pulsar or soft gamma-ray repeater with detected pulsations"));
	}

	if (pcode.contains("BINARY") || bperiod>0)
	{
		out.append(q_("has one or more binary companions"));
	}

	if (pcode.contains("HE"))
	{
		out.append(q_("with pulsed emission from radio to infrared or higher frequencies"));
	}

	if (pcode.contains("NRAD"))
	{
		out.append(q_("with pulsed emission only at infrared or higher frequencies"));
	}

	if (pcode.contains("RADIO"))
	{
		out.append(q_("with pulsed emission in the radio band"));
	}

	if (pcode.contains("RRAT"))
	{
		out.append(q_("with intermittently pulsed radio emission"));
	}

	if (pcode.contains("XINS"))
	{
		out.append(q_("isolated neutron star with pulsed thermal X-ray emission but no detectable radio emission"));
	}

	return out.join(",<br />");
}

double Pulsar::getAngularSize(const StelCore*) const
{
	return 0.00001;
}

void Pulsar::update(double deltaTime)
{
	labelsFader.update((int)(deltaTime*1000));
}

void Pulsar::draw(StelCore* core, StelPainter *painter)
{
	StelSkyDrawer* sd = core->getSkyDrawer();
	double mag = getVMagnitudeWithExtinction(core);
	StelUtils::spheToRect(RA, DE, XYZ);

	Vec3d win;
	// Check visibility of pulsar
	if (!(painter->getProjector()->projectCheck(XYZ, win)))
		return;

	painter->setBlending(true, GL_ONE, GL_ONE);

	if (glitch>0 && glitchFlag)
		painter->setColor(glitchColor[0], glitchColor[1], glitchColor[2], 1.f);
	else
		painter->setColor(markerColor[0], markerColor[1], markerColor[2], 1.f);
	float mlimit = sd->getLimitMagnitude();

	if (mag <= mlimit)
	{		
		Pulsar::markerTexture->bind();
		float size = getAngularSize(NULL)*M_PI/180.*painter->getProjector()->getPixelPerRadAtCenter();
		float shift = 5.f + size/1.6f;		

		painter->drawSprite2dMode(XYZ, distributionMode ? 4.f : 5.f);

		if (labelsFader.getInterstate()<=0.f && !distributionMode && (mag+2.f)<mlimit)
		{
			painter->drawText(XYZ, designation, 0, shift, shift, false);
		}
	}
}
