import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterLink } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { ApiService, WorkshopItem, User } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

interface AdminStats {
  total_items: number;
  total_reports: number;
  total_users: number;
  hidden_items: number;
  reported_items: number;
  total_downloads: number;
}

interface AdminReport {
  report_id: number;
  item_id: string;
  client_uuid: string;
  reason: 'broken' | 'offensive' | 'spam' | 'other';
  details: string;
  reported_at: string;
  item_title?: string;
  item_type?: string;
  item_author?: string;
  reports_count?: number;
  item_is_hidden?: number;
  item_version?: number;
  item_thumbnail_path?: string;
}

@Component({
  selector: 'app-admin',
  standalone: true,
  imports: [CommonModule, RouterLink, FormsModule],
  templateUrl: './admin.html',
  styleUrl: './admin.scss'
})
export class AdminComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);

  isAuthorized = signal<boolean>(false);
  authChecking = signal<boolean>(true);
  adminKeyInput = signal<string>(localStorage.getItem('sand3_admin_key') || '');
  authError = signal<string>('');

  activeTab = signal<'reports' | 'items' | 'users' | 'stats'>('reports');

  // Stats
  stats = signal<AdminStats | null>(null);
  statsLoading = signal<boolean>(false);

  // Reports
  reports = signal<AdminReport[]>([]);
  reportsLoading = signal<boolean>(false);

  // Manage Items
  items = signal<WorkshopItem[]>([]);
  itemsLoading = signal<boolean>(false);
  searchQuery = signal<string>('');
  typeFilter = signal<string>('all');
  statusFilter = signal<'all' | 'reported' | 'hidden'>('all');

  // Manage Users
  users = signal<User[]>([]);
  usersLoading = signal<boolean>(false);
  usersSearchQuery = signal<string>('');

  // Action status notification
  toastMsg = signal<string>('');
  toastType = signal<'success' | 'danger'>('success');

  ngOnInit() {
    this.checkAccess();
  }

  showToast(msg: string, type: 'success' | 'danger' = 'success') {
    this.toastMsg.set(msg);
    this.toastType.set(type);
    setTimeout(() => {
      if (this.toastMsg() === msg) {
        this.toastMsg.set('');
      }
    }, 3500);
  }

  checkAccess() {
    this.authChecking.set(true);
    this.authError.set('');

    this.api.checkAdmin().subscribe({
      next: (res) => {
        this.authChecking.set(false);
        if (res.is_admin || res.is_moderator || this.auth.isModerator()) {
          this.isAuthorized.set(true);
          this.loadStats();
          this.loadReports();
          this.loadItems();
          this.loadUsers();
        } else {
          this.isAuthorized.set(false);
        }
      },
      error: () => {
        this.authChecking.set(false);
        this.isAuthorized.set(false);
      }
    });
  }

  unlockWithKey() {
    const key = this.adminKeyInput().trim();
    if (!key) {
      this.authError.set('Please enter an admin key');
      return;
    }

    localStorage.setItem('sand3_admin_key', key);
    this.checkAccess();
  }

  loadStats() {
    this.statsLoading.set(true);
    this.api.getAdminStats().subscribe({
      next: (data) => {
        this.stats.set(data);
        this.statsLoading.set(false);
      },
      error: (e) => {
        console.error('Failed to load admin stats', e);
        this.statsLoading.set(false);
      }
    });
  }

  loadReports() {
    this.reportsLoading.set(true);
    this.api.getAdminReports().subscribe({
      next: (res) => {
        this.reports.set(res.reports || []);
        this.reportsLoading.set(false);
      },
      error: (e) => {
        console.error('Failed to load reports', e);
        this.reportsLoading.set(false);
      }
    });
  }

  loadItems() {
    this.itemsLoading.set(true);
    this.api
      .getAdminItems(this.searchQuery(), this.typeFilter(), this.statusFilter())
      .subscribe({
        next: (res) => {
          this.items.set(res.items || []);
          this.itemsLoading.set(false);
        },
        error: (e) => {
          console.error('Failed to load items', e);
          this.itemsLoading.set(false);
        }
      });
  }

  loadUsers() {
    this.usersLoading.set(true);
    this.api.getAdminUsers(this.usersSearchQuery()).subscribe({
      next: (res) => {
        this.users.set(res.users || []);
        this.usersLoading.set(false);
      },
      error: (e) => {
        console.error('Failed to load users', e);
        this.usersLoading.set(false);
      }
    });
  }

  toggleUserAdmin(user: User) {
    const actionText = user.is_admin ? 'revoke administrator privileges from' : 'grant administrator privileges to';
    if (!confirm(`Are you sure you want to ${actionText} user "${user.username}"?`)) {
      return;
    }

    this.api.toggleUserAdmin(user.id).subscribe({
      next: (res) => {
        this.users.update((list) =>
          list.map((u) => (u.id === user.id ? { ...u, is_admin: res.is_admin ? 1 : 0 } : u))
        );
        this.showToast(res.message);
      },
      error: (e) => {
        this.showToast(e.error?.error || e.message, 'danger');
      }
    });
  }

  changeUserRole(user: User, newRole: 'admin' | 'moderator' | 'user') {
    const roleName = newRole === 'admin' ? 'Administrator' : (newRole === 'moderator' ? 'Moderator' : 'regular User');
    if (!confirm(`Are you sure you want to set "${user.username}" to ${roleName}?`)) {
      return;
    }

    this.api.setUserRole(user.id, newRole).subscribe({
      next: (res) => {
        this.users.update((list) =>
          list.map((u) => (u.id === user.id ? { ...u, role: newRole, is_admin: res.is_admin ? 1 : 0 } : u))
        );
        this.showToast(res.message);
      },
      error: (e) => {
        this.showToast(e.error?.error || e.message, 'danger');
      }
    });
  }

  userDelete(user: User) {
    if (!confirm(`Are you sure you want to permanently delete user "${user.username}"? This cannot be undone.`)) {
      return;
    }

    this.api.userDelete(user.id).subscribe({
      next: (res) => {
        this.users.update((list) => list.filter((u) => u.id !== user.id));
        this.showToast(res.message || `User "${user.username}" was deleted.`);
        this.loadStats();
      },
      error: (e) => {
        this.showToast(e.error?.error || e.message, 'danger');
      }
    });
  }

  dismissReport(reportId: number) {
    this.api.dismissReport(reportId).subscribe({
      next: () => {
        this.reports.update((list) => list.filter((r) => r.report_id !== reportId));
        this.showToast('Report dismissed successfully.');
        this.loadStats();
      },
      error: (e) => {
        this.showToast('Failed to dismiss report: ' + (e.error?.error || e.message), 'danger');
      }
    });
  }

  clearItemReports(itemId: string) {
    this.api.clearItemReports(itemId).subscribe({
      next: () => {
        this.reports.update((list) => list.filter((r) => r.item_id !== itemId));
        this.items.update((list) =>
          list.map((it) => (it.id === itemId ? { ...it, reports_count: 0, is_hidden: 0 } : it))
        );
        this.showToast('All reports cleared and item unhidden.');
        this.loadStats();
      },
      error: (e) => {
        this.showToast('Failed to clear reports: ' + (e.error?.error || e.message), 'danger');
      }
    });
  }

  toggleHideItem(item: WorkshopItem | AdminReport) {
    const id = 'id' in item ? item.id : item.item_id;
    this.api.toggleHideItem(id).subscribe({
      next: (res) => {
        const newHidden = res.is_hidden;
        this.items.update((list) =>
          list.map((it) => (it.id === id ? { ...it, is_hidden: newHidden } : it))
        );
        this.reports.update((list) =>
          list.map((r) => (r.item_id === id ? { ...r, item_is_hidden: newHidden } : r))
        );
        this.showToast(res.message || 'Visibility updated.');
        this.loadStats();
      },
      error: (e) => {
        this.showToast('Failed to toggle visibility: ' + (e.error?.error || e.message), 'danger');
      }
    });
  }

  deleteItem(itemId: string, title?: string) {
    const displayTitle = title || itemId;
    if (!confirm(`ADMIN ACTION: Are you sure you want to permanently delete "${displayTitle}" from the workshop? This will remove all associated files and reports.`)) {
      return;
    }

    this.api.adminDeleteItem(itemId).subscribe({
      next: () => {
        this.items.update((list) => list.filter((it) => it.id !== itemId));
        this.reports.update((list) => list.filter((r) => r.item_id !== itemId));
        this.showToast(`Item "${displayTitle}" deleted permanently.`);
        this.loadStats();
      },
      error: (e) => {
        this.showToast('Failed to delete item: ' + (e.error?.error || e.message), 'danger');
      }
    });
  }

  getThumbnailUrl(itemId: string): string {
    return this.api.getThumbnailUrl(itemId);
  }
}
