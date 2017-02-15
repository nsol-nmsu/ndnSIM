
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- *//**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_AGGREGATOR_H
#define NDN_AGGREGATOR_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-app.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include "ns3/random-variable-stream.h"
#include "ns3/ndnSIM/utils/ndn-rtt-estimator.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief An application that concatenates payloaded interests at the aggregation
 * of the Smart Grid architecture. At specified intervals (configurable by user), a single payload interest
 * is sent upstream to the compute layer. The size of the payload equals the total
 * bytes received within the configured wait interval. Install on nodes at the aggregation layer
 * of the Smart Grid architecture (iCenS)
 */
class Aggregator : public App {
public:
  static TypeId
  GetTypeId(void);

  Aggregator();

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  // From App
  virtual void
  OnData(shared_ptr<const Data> contentObject);

  /**
   * @brief Aggregate payloads from all received interests and forward upstream
   */
  void
  ScheduleAggPackets();

  /**
   * @brief Actually send aggregated payloaded interest. The payload
   * size is the total bytes received in the configured time interval
   */
  void
  SendAggInterest();

  /**
   * @brief An event that is fired just before an Interest packet is actually send out (send is
   *inevitable)
   *
   * The reason for "before" even is that in certain cases (when it is possible to satisfy from the
   *local cache),
   * the send call will immediately return data, and if "after" even was used, this after would be
   *called after
   * all processing of incoming data, potentially producing unexpected results.
   */
  virtual void
  WillSendOutInterest(uint32_t sequenceNumber);

  /**
   * @brief Timeout event
   * @param sequenceNumber time outed sequence number
   */
  virtual void
  OnTimeout(uint32_t sequenceNumber);

public:
  typedef void (*ReceivedInterestTraceCallback)( uint32_t, shared_ptr<const Interest> );
  typedef void (*SentInterestTraceCallback)( uint32_t, shared_ptr<const Interest> );

protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

  /**
   * \brief Checks if the packet need to be retransmitted becuase of retransmission timer expiration
   */
  void
  CheckRetxTimeout();

  /**
   * \brief Modifies the frequency of checking the retransmission timeouts
   * \param retxTimer Timeout defining how frequent retransmission timeouts should be checked
   */
  void
  SetRetxTimer(Time retxTimer);

  /**
   * \brief Returns the frequency of checking the retransmission timeouts
   * \return Timeout defining how frequent retransmission timeouts should be checked
   */
  Time
  GetRetxTimer() const;

private:
  Name m_prefix;
  Name m_postfix;
  uint32_t m_virtualPayloadSize;
  Time m_freshness;

  uint32_t m_signature;
  Name m_keyLocator;

  size_t m_totalpayload;
  Time m_frequency;
  EventId m_txEvent;
  bool m_firstTime;
  Name m_upstream_prefix; ///< \brief NDN Name of the Interest (use Name)
  uint32_t m_offset; //random offset

  /// @cond include_hidden
  /**
   * \struct This struct contains sequence numbers of packets to be retransmitted
   */
  struct RetxSeqsContainer : public std::set<uint32_t> {
  };

  RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted
  uint32_t m_seq;      ///< @brief currently requested sequence number
  uint32_t m_seqMax;   ///< @brief maximum number of sequence number
  Time m_interestLifeTime; ///< \brief LifeTime for interest packet
  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator

  /**
   * \struct This struct contains a pair of packet sequence number and its timeout
   */
  struct SeqTimeout {
    SeqTimeout(uint32_t _seq, Time _time)
      : seq(_seq)
      , time(_time)
    {
    }

    uint32_t seq;
    Time time;
  };
  /// @endcond

  /// @cond include_hidden
  class i_seq {
  };
  class i_timestamp {
  };
  /// @endcond

  /// @cond include_hidden
   /**
   * \struct This struct contains a multi-index for the set of SeqTimeout structs
   */
  struct SeqTimeoutsContainer
    : public boost::multi_index::
        multi_index_container<SeqTimeout,
                              boost::multi_index::
                                indexed_by<boost::multi_index::
                                             ordered_unique<boost::multi_index::tag<i_seq>,
                                                            boost::multi_index::
                                                              member<SeqTimeout, uint32_t,
                                                                     &SeqTimeout::seq>>,
                                           boost::multi_index::
                                             ordered_non_unique<boost::multi_index::
                                                                  tag<i_timestamp>,
                                                                boost::multi_index::
                                                                  member<SeqTimeout, Time,
                                                                         &SeqTimeout::time>>>> {
  };
  /// @endcond

  SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs

  SeqTimeoutsContainer m_seqLastDelay;
  SeqTimeoutsContainer m_seqFullDelay;

  std::map<uint32_t, uint32_t> m_seqRetxCounts;
  Ptr<RttEstimator> m_rtt; ///< @brief RTT estimator
  Time m_retxTimer;    ///< @brief Currently estimated retransmission timer
  EventId m_retxEvent; ///< @brief Event to check whether or not retransmission should be performed
  Time m_agg_offset;

protected:
  TracedCallback <  uint32_t, shared_ptr<const Interest> > m_receivedInterest;
  TracedCallback <  uint32_t, shared_ptr<const Interest> > m_sentInterest;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_AGGREGATOR_H
