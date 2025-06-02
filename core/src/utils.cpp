/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Hamburg University
 *  Copyright (c) 2017, Bielefeld University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Bielefeld University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Authors: Michael Goerner, Robert Haschke */

#if __has_include(<tf2_eigen/tf2_eigen.hpp>)
#include <tf2_eigen/tf2_eigen.hpp>
#else
#include <tf2_eigen/tf2_eigen.h>
#endif

#include <moveit/robot_state/robot_state.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/robot_trajectory/robot_trajectory.h>
#include <moveit/collision_detection/collision_tools.h>

#include <moveit/task_constructor/properties.h>
#include <moveit/task_constructor/storage.h>

namespace moveit {
namespace task_constructor {
namespace utils {

bool getRobotTipForFrame(const Property& property, const planning_scene::PlanningScene& scene,
                         const moveit::core::JointModelGroup* jmg, std::string& error_msg,
                         const moveit::core::LinkModel*& robot_link, Eigen::Isometry3d& tip_in_global_frame) {
	auto get_tip = [&jmg]() -> const moveit::core::LinkModel* {
		// determine IK frame from group
		std::vector<const moveit::core::LinkModel*> tips;
		jmg->getEndEffectorTips(tips);
		if (tips.size() != 1) {
			return nullptr;
		}
		return tips[0];
	};

	error_msg = "";

	if (property.value().empty()) {  // property undefined
		robot_link = get_tip();
		if (!robot_link) {
			error_msg = "missing ik_frame";
			return false;
		}
		tip_in_global_frame = scene.getCurrentState().getGlobalLinkTransform(robot_link);
	} else {
		auto ik_pose_msg = boost::any_cast<geometry_msgs::msg::PoseStamped>(property.value());
		tf2::fromMsg(ik_pose_msg.pose, tip_in_global_frame);

		robot_link = nullptr;
		bool found = false;
		auto ref_frame = scene.getCurrentState().getFrameInfo(ik_pose_msg.header.frame_id, robot_link, found);
		if (!found && !ik_pose_msg.header.frame_id.empty()) {
			std::stringstream ss;
			ss << "ik_frame specified in unknown frame '" << ik_pose_msg.header.frame_id << "'";
			error_msg = ss.str();
			return false;
		}
		if (!robot_link)
			robot_link = get_tip();
		if (!robot_link) {
			error_msg = "ik_frame doesn't specify a link frame";
			return false;
		} else if (!found) {  // use robot link's frame as reference by default
			ref_frame = scene.getCurrentState().getGlobalLinkTransform(robot_link);
		}

		tip_in_global_frame = ref_frame * tip_in_global_frame;
	}

	return true;
}

void markPathCollisions(const robot_trajectory::RobotTrajectory& trajectory,
                        const planning_scene::PlanningSceneConstPtr& planning_scene,
                        const moveit_msgs::msg::Constraints& path_constraints, const std::string& group_name,
                        visualization_msgs::msg::MarkerArray& markers_out) {
	std::size_t n_wp = trajectory.getWayPointCount();
	for (std::size_t it = 0; it < n_wp; ++it) {
		const moveit::core::RobotState& robot_state = trajectory.getWayPoint(it);

		// Collision contact check
		collision_detection::CollisionRequest c_req;
		collision_detection::CollisionResult c_res;
		c_req.contacts = true;
		c_req.max_contacts = 10;
		c_req.max_contacts_per_pair = 3;
		c_req.verbose = true;

		planning_scene->checkCollision(c_req, c_res, robot_state);

		if (c_res.contact_count > 0) {
			collision_detection::getCollisionMarkersFromContacts(markers_out, planning_scene->getPlanningFrame(),
			                                                     c_res.contacts);
		}
	}
}

}  // namespace utils
}  // namespace task_constructor
}  // namespace moveit
