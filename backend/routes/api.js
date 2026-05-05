const express = require('express');
const router = express.Router();
const dashboardController = require('../controllers/dashboardController');

router.get('/slots', dashboardController.getSlots);
router.get('/events', dashboardController.getEvents);
router.get('/sessions', dashboardController.getSessions);
router.get('/dashboard/stats', dashboardController.getDashboardStats);
router.get('/database/status', dashboardController.getDatabaseStatus);
router.get('/dashboard/trend', dashboardController.getWeeklyTrend);

module.exports = router;
