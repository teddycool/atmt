-- phpMyAdmin SQL Dump
-- version 5.2.1deb1
-- https://www.phpmyadmin.net/
--
-- Host: localhost:3306
-- Generation Time: May 08, 2024 at 06:08 PM
-- Server version: 10.11.6-MariaDB-0+deb12u1
-- PHP Version: 8.2.18

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `scaniahack`
--
CREATE DATABASE IF NOT EXISTS `scaniahack` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `scaniahack`;

-- --------------------------------------------------------

--
-- Table structure for table `Logging`
--

CREATE TABLE `Logging` (
  `id` int(11) NOT NULL,
  `time` datetime NOT NULL DEFAULT current_timestamp(),
  `chipid` char(14) NOT NULL,
  `msg` varchar(100) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `Logging`
--

INSERT INTO `Logging` (`id`, `time`, `chipid`, `msg`) VALUES
(406, '2024-05-08 20:05:43', '64b7084cff5c', 'OTA server started'),
(407, '2024-05-08 20:05:43', '64b7084cff5c', 'ATMT started!'),
(408, '2024-05-08 20:05:44', '64b7084cff5c', 'Front distance: 115.46 cm'),
(409, '2024-05-08 20:05:44', '64b7084cff5c', 'Rear distance: 88.87 cm'),
(410, '2024-05-08 20:05:44', '64b7084cff5c', 'Right distance: 16.53 cm'),
(411, '2024-05-08 20:05:44', '64b7084cff5c', 'Left distance: 65.98 cm'),
(412, '2024-05-08 20:05:44', '64b7084cff5c', 'Reading the accelerometer values'),
(413, '2024-05-08 20:05:46', '64b7084cff5c', 'Front distance: 0.00 cm'),
(414, '2024-05-08 20:05:47', '64b7084cff5c', 'Rear distance: 131.03 cm'),
(415, '2024-05-08 20:05:47', '64b7084cff5c', 'Right distance: 152.96 cm'),
(416, '2024-05-08 20:05:47', '64b7084cff5c', 'Left distance: 68.18 cm'),
(417, '2024-05-08 20:05:47', '64b7084cff5c', 'Reading the accelerometer values'),
(418, '2024-05-08 20:05:49', '64b7084cff5c', 'Front distance: 0.00 cm'),
(419, '2024-05-08 20:05:49', '64b7084cff5c', 'Rear distance: 121.68 cm'),
(420, '2024-05-08 20:05:49', '64b7084cff5c', 'Right distance: 81.86 cm'),
(421, '2024-05-08 20:05:49', '64b7084cff5c', 'Left distance: 115.88 cm'),
(422, '2024-05-08 20:05:49', '64b7084cff5c', 'Reading the accelerometer values'),
(423, '2024-05-08 20:05:52', '64b7084cff5c', 'Front distance: 0.00 cm'),
(424, '2024-05-08 20:05:52', '64b7084cff5c', 'Rear distance: 84.98 cm'),
(425, '2024-05-08 20:05:52', '64b7084cff5c', 'Right distance: 77.42 cm'),
(426, '2024-05-08 20:05:52', '64b7084cff5c', 'Left distance: 109.38 cm'),
(427, '2024-05-08 20:05:52', '64b7084cff5c', 'Reading the accelerometer values'),
(428, '2024-05-08 20:05:55', '64b7084cff5c', 'Front distance: 0.00 cm'),
(429, '2024-05-08 20:05:55', '64b7084cff5c', 'Rear distance: 75.88 cm'),
(430, '2024-05-08 20:05:55', '64b7084cff5c', 'Right distance: 152.68 cm'),
(431, '2024-05-08 20:05:55', '64b7084cff5c', 'Left distance: 110.31 cm'),
(432, '2024-05-08 20:05:55', '64b7084cff5c', 'Reading the accelerometer values'),
(433, '2024-05-08 20:05:58', '64b7084cff5c', 'Front distance: 139.52 cm'),
(434, '2024-05-08 20:05:58', '64b7084cff5c', 'Rear distance: 75.60 cm'),
(435, '2024-05-08 20:05:58', '64b7084cff5c', 'Right distance: 120.00 cm'),
(436, '2024-05-08 20:05:58', '64b7084cff5c', 'Left distance: 0.00 cm'),
(437, '2024-05-08 20:05:58', '64b7084cff5c', 'Reading the accelerometer values'),
(438, '2024-05-08 20:06:01', '64b7084cff5c', 'Front distance: 141.31 cm'),
(439, '2024-05-08 20:06:01', '64b7084cff5c', 'Rear distance: 75.57 cm'),
(440, '2024-05-08 20:06:01', '64b7084cff5c', 'Right distance: 0.00 cm'),
(441, '2024-05-08 20:06:01', '64b7084cff5c', 'Left distance: 0.00 cm'),
(442, '2024-05-08 20:06:01', '64b7084cff5c', 'Reading the accelerometer values'),
(443, '2024-05-08 20:06:03', '64b7084cff5c', 'Front distance: 139.18 cm'),
(444, '2024-05-08 20:06:04', '64b7084cff5c', 'Rear distance: 75.57 cm'),
(445, '2024-05-08 20:06:04', '64b7084cff5c', 'Right distance: 143.75 cm'),
(446, '2024-05-08 20:06:04', '64b7084cff5c', 'Left distance: 0.00 cm'),
(447, '2024-05-08 20:06:04', '64b7084cff5c', 'Reading the accelerometer values'),
(448, '2024-05-08 20:06:06', '64b7084cff5c', 'Front distance: 139.62 cm'),
(449, '2024-05-08 20:06:06', '64b7084cff5c', 'Rear distance: 75.53 cm'),
(450, '2024-05-08 20:06:06', '64b7084cff5c', 'Right distance: 118.69 cm'),
(451, '2024-05-08 20:06:06', '64b7084cff5c', 'Left distance: 0.00 cm'),
(452, '2024-05-08 20:06:06', '64b7084cff5c', 'Reading the accelerometer values'),
(453, '2024-05-08 20:06:09', '64b7084cff5c', 'Front distance: 140.07 cm'),
(454, '2024-05-08 20:06:09', '64b7084cff5c', 'Rear distance: 75.57 cm'),
(455, '2024-05-08 20:06:09', '64b7084cff5c', 'Right distance: 118.73 cm'),
(456, '2024-05-08 20:06:09', '64b7084cff5c', 'Left distance: 0.00 cm'),
(457, '2024-05-08 20:06:09', '64b7084cff5c', 'Reading the accelerometer values'),
(458, '2024-05-08 20:06:12', '64b7084cff5c', 'Front distance: 139.62 cm'),
(459, '2024-05-08 20:06:12', '64b7084cff5c', 'Rear distance: 75.64 cm'),
(460, '2024-05-08 20:06:12', '64b7084cff5c', 'Right distance: 121.24 cm'),
(461, '2024-05-08 20:06:12', '64b7084cff5c', 'Left distance: 0.00 cm'),
(462, '2024-05-08 20:06:12', '64b7084cff5c', 'Reading the accelerometer values'),
(463, '2024-05-08 20:06:15', '64b7084cff5c', 'Front distance: 139.97 cm'),
(464, '2024-05-08 20:06:15', '64b7084cff5c', 'Rear distance: 76.05 cm'),
(465, '2024-05-08 20:06:15', '64b7084cff5c', 'Right distance: 142.61 cm'),
(466, '2024-05-08 20:06:15', '64b7084cff5c', 'Left distance: 0.00 cm'),
(467, '2024-05-08 20:06:15', '64b7084cff5c', 'Reading the accelerometer values'),
(468, '2024-05-08 20:06:18', '64b7084cff5c', 'Front distance: 141.24 cm'),
(469, '2024-05-08 20:06:18', '64b7084cff5c', 'Rear distance: 75.67 cm'),
(470, '2024-05-08 20:06:18', '64b7084cff5c', 'Right distance: 129.79 cm'),
(471, '2024-05-08 20:06:18', '64b7084cff5c', 'Left distance: 0.00 cm'),
(472, '2024-05-08 20:06:18', '64b7084cff5c', 'Reading the accelerometer values'),
(473, '2024-05-08 20:06:21', '64b7084cff5c', 'Front distance: 139.48 cm'),
(474, '2024-05-08 20:06:21', '64b7084cff5c', 'Rear distance: 75.67 cm'),
(475, '2024-05-08 20:06:21', '64b7084cff5c', 'Right distance: 87.29 cm'),
(476, '2024-05-08 20:06:21', '64b7084cff5c', 'Left distance: 0.00 cm'),
(477, '2024-05-08 20:06:21', '64b7084cff5c', 'Reading the accelerometer values'),
(478, '2024-05-08 20:06:23', '64b7084cff5c', 'Front distance: 0.00 cm'),
(479, '2024-05-08 20:06:23', '64b7084cff5c', 'Rear distance: 90.86 cm'),
(480, '2024-05-08 20:06:23', '64b7084cff5c', 'Right distance: 6.05 cm'),
(481, '2024-05-08 20:06:23', '64b7084cff5c', 'Left distance: 8.63 cm'),
(482, '2024-05-08 20:06:24', '64b7084cff5c', 'Reading the accelerometer values'),
(483, '2024-05-08 20:06:26', '64b7084cff5c', 'Front distance: 110.00 cm'),
(484, '2024-05-08 20:06:26', '64b7084cff5c', 'Rear distance: 107.25 cm'),
(485, '2024-05-08 20:06:26', '64b7084cff5c', 'Right distance: 92.51 cm'),
(486, '2024-05-08 20:06:26', '64b7084cff5c', 'Left distance: 58.87 cm'),
(487, '2024-05-08 20:06:26', '64b7084cff5c', 'Reading the accelerometer values'),
(488, '2024-05-08 20:06:29', '64b7084cff5c', 'Front distance: 0.00 cm'),
(489, '2024-05-08 20:06:29', '64b7084cff5c', 'Rear distance: 109.24 cm'),
(490, '2024-05-08 20:06:29', '64b7084cff5c', 'Right distance: 77.35 cm'),
(491, '2024-05-08 20:06:29', '64b7084cff5c', 'Left distance: 68.87 cm'),
(492, '2024-05-08 20:06:29', '64b7084cff5c', 'Reading the accelerometer values'),
(493, '2024-05-08 20:06:32', '64b7084cff5c', 'Front distance: 0.00 cm'),
(494, '2024-05-08 20:06:32', '64b7084cff5c', 'Rear distance: 131.48 cm'),
(495, '2024-05-08 20:06:32', '64b7084cff5c', 'Right distance: 0.00 cm'),
(496, '2024-05-08 20:06:32', '64b7084cff5c', 'Left distance: 146.15 cm'),
(497, '2024-05-08 20:06:32', '64b7084cff5c', 'Reading the accelerometer values'),
(498, '2024-05-08 20:06:35', '64b7084cff5c', 'Front distance: 143.30 cm'),
(499, '2024-05-08 20:06:35', '64b7084cff5c', 'Rear distance: 77.22 cm'),
(500, '2024-05-08 20:06:35', '64b7084cff5c', 'Right distance: 0.00 cm'),
(501, '2024-05-08 20:06:35', '64b7084cff5c', 'Left distance: 112.13 cm'),
(502, '2024-05-08 20:06:35', '64b7084cff5c', 'Reading the accelerometer values'),
(503, '2024-05-08 20:06:38', '64b7084cff5c', 'Front distance: 139.76 cm'),
(504, '2024-05-08 20:06:38', '64b7084cff5c', 'Rear distance: 76.36 cm'),
(505, '2024-05-08 20:06:38', '64b7084cff5c', 'Right distance: 112.44 cm'),
(506, '2024-05-08 20:06:38', '64b7084cff5c', 'Left distance: 0.00 cm'),
(507, '2024-05-08 20:06:38', '64b7084cff5c', 'Reading the accelerometer values'),
(508, '2024-05-08 20:06:40', '64b7084cff5c', 'Front distance: 142.41 cm'),
(509, '2024-05-08 20:06:40', '64b7084cff5c', 'Rear distance: 76.36 cm'),
(510, '2024-05-08 20:06:40', '64b7084cff5c', 'Right distance: 125.67 cm'),
(511, '2024-05-08 20:06:40', '64b7084cff5c', 'Left distance: 0.00 cm'),
(512, '2024-05-08 20:06:41', '64b7084cff5c', 'Reading the accelerometer values'),
(513, '2024-05-08 20:06:43', '64b7084cff5c', 'Front distance: 141.51 cm'),
(514, '2024-05-08 20:06:43', '64b7084cff5c', 'Rear distance: 75.98 cm'),
(515, '2024-05-08 20:06:43', '64b7084cff5c', 'Right distance: 112.37 cm'),
(516, '2024-05-08 20:06:43', '64b7084cff5c', 'Left distance: 0.00 cm'),
(517, '2024-05-08 20:06:43', '64b7084cff5c', 'Reading the accelerometer values'),
(518, '2024-05-08 20:06:46', '64b7084cff5c', 'Front distance: 140.17 cm'),
(519, '2024-05-08 20:06:46', '64b7084cff5c', 'Rear distance: 76.05 cm'),
(520, '2024-05-08 20:06:46', '64b7084cff5c', 'Right distance: 127.70 cm'),
(521, '2024-05-08 20:06:46', '64b7084cff5c', 'Left distance: 152.61 cm'),
(522, '2024-05-08 20:06:46', '64b7084cff5c', 'Reading the accelerometer values'),
(523, '2024-05-08 20:06:49', '64b7084cff5c', 'Front distance: 139.66 cm'),
(524, '2024-05-08 20:06:49', '64b7084cff5c', 'Rear distance: 76.53 cm'),
(525, '2024-05-08 20:06:49', '64b7084cff5c', 'Right distance: 126.87 cm'),
(526, '2024-05-08 20:06:49', '64b7084cff5c', 'Left distance: 0.00 cm'),
(527, '2024-05-08 20:06:49', '64b7084cff5c', 'Reading the accelerometer values'),
(528, '2024-05-08 20:06:52', '64b7084cff5c', 'Front distance: 140.86 cm'),
(529, '2024-05-08 20:06:52', '64b7084cff5c', 'Rear distance: 76.22 cm'),
(530, '2024-05-08 20:06:52', '64b7084cff5c', 'Right distance: 112.27 cm'),
(531, '2024-05-08 20:06:52', '64b7084cff5c', 'Left distance: 152.47 cm'),
(532, '2024-05-08 20:06:52', '64b7084cff5c', 'Reading the accelerometer values'),
(533, '2024-05-08 20:06:55', '64b7084cff5c', 'Front distance: 141.27 cm'),
(534, '2024-05-08 20:06:55', '64b7084cff5c', 'Rear distance: 77.08 cm'),
(535, '2024-05-08 20:06:55', '64b7084cff5c', 'Right distance: 110.24 cm'),
(536, '2024-05-08 20:06:55', '64b7084cff5c', 'Left distance: 153.92 cm'),
(537, '2024-05-08 20:06:55', '64b7084cff5c', 'Reading the accelerometer values'),
(538, '2024-05-08 20:06:57', '64b7084cff5c', 'Front distance: 139.55 cm'),
(539, '2024-05-08 20:06:57', '64b7084cff5c', 'Rear distance: 76.67 cm'),
(540, '2024-05-08 20:06:57', '64b7084cff5c', 'Right distance: 66.43 cm'),
(541, '2024-05-08 20:06:58', '64b7084cff5c', 'Left distance: 152.23 cm'),
(542, '2024-05-08 20:06:58', '64b7084cff5c', 'Reading the accelerometer values');

-- --------------------------------------------------------

--
-- Table structure for table `Measurement`
--

CREATE TABLE `Measurement` (
  `id` int(11) NOT NULL,
  `name` char(20) NOT NULL,
  `value` float NOT NULL,
  `unit` char(10) DEFAULT NULL,
  `time` datetime NOT NULL,
  `chipid` char(14) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `Logging`
--
ALTER TABLE `Logging`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `Measurement`
--
ALTER TABLE `Measurement`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `Logging`
--
ALTER TABLE `Logging`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=543;

--
-- AUTO_INCREMENT for table `Measurement`
--
ALTER TABLE `Measurement`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
