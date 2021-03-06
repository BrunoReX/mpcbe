/*
 * (C) 2006-2015 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "Packet.h"

//
// CPacketQueue
//

CPacketQueue::CPacketQueue()
{
}

void CPacketQueue::Add(CAutoPtr<CPacket> p)
{
	CAutoLock cAutoLock(this);

	if (p) {
		m_size += p->GetCount();
	}

	AddTail(p);
}

CAutoPtr<CPacket> CPacketQueue::Remove()
{
	CAutoLock cAutoLock(this);
	ASSERT(__super::GetCount() > 0);
	CAutoPtr<CPacket> p = RemoveHead();
	if (p) {
		m_size -= p->GetCount();
	}
	return p;
}

void CPacketQueue::RemoveAll()
{
	CAutoLock cAutoLock(this);
	m_size = 0;
	__super::RemoveAll();
}

size_t CPacketQueue::GetCount()
{
	CAutoLock cAutoLock(this);
	return __super::GetCount();
}

size_t CPacketQueue::GetSize()
{
	CAutoLock cAutoLock(this);
	return m_size;
}

REFERENCE_TIME CPacketQueue::GetDuration()
{
	CAutoLock cAutoLock(this);
	return GetCount() ? (GetTail()->rtStop - GetHead()->rtStart) : 0;
}
