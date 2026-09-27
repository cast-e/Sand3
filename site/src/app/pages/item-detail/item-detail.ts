import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { ApiService, WorkshopItem } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

@Component({
  selector: 'app-item-detail',
  standalone: true,
  imports: [CommonModule, RouterLink, FormsModule],
  templateUrl: './item-detail.html',
  styleUrl: './item-detail.scss'
})
export class ItemDetailComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  item = signal<WorkshopItem | null>(null);
  loading = signal<boolean>(true);
  errorMsg = signal<string>('');

  // Edit Modal State
  editingItem = signal<boolean>(false);
  editTitle = signal<string>('');
  editDescription = signal<string>('');
  editLoading = signal<boolean>(false);
  editError = signal<string>('');

  // Report Modal State
  reportingItem = signal<boolean>(false);
  reportReason = signal<'broken' | 'offensive' | 'spam' | 'other'>('broken');
  reportDetails = signal<string>('');
  reportLoading = signal<boolean>(false);
  reportSuccess = signal<string>('');
  reportError = signal<string>('');

  ngOnInit() {
    this.route.params.subscribe((params) => {
      const id = params['id'];
      if (id) {
        this.loadItem(id);
      }
    });
  }

  loadItem(id: string) {
    this.loading.set(true);
    this.errorMsg.set('');
    this.api.getItem(id).subscribe({
      next: (data) => {
        this.item.set(data);
        this.loading.set(false);
      },
      error: (err) => {
        console.error('Failed to load item', err);
        this.errorMsg.set('Item not found or failed to load.');
        this.loading.set(false);
      }
    });
  }

  isOwner(): boolean {
    const it = this.item();
    const user = this.auth.currentUser();
    if (!it || !user) return false;
    if (it.user_id && it.user_id === user.id) return true;
    if (!it.user_id && it.author.toLowerCase() === user.username.toLowerCase()) return true;
    return false;
  }

  isAdmin(): boolean {
    return this.auth.isAdmin();
  }

  canManage(): boolean {
    return this.isOwner() || this.isAdmin();
  }

  toggleLike() {
    const it = this.item();
    if (!it) return;
    this.api.toggleLike(it.id).subscribe((res) => {
      this.item.update((val) => (val ? { ...val, is_liked: res.is_liked, likes_count: res.likes_count } : null));
    });
  }

  toggleFavorite() {
    const it = this.item();
    if (!it) return;
    this.api.toggleFavorite(it.id).subscribe((res) => {
      this.item.update((val) => (val ? { ...val, is_favorited: res.is_favorited, favorites_count: res.favorites_count } : null));
    });
  }

  // Usage Guide Popup State
  showUsageModal = signal<boolean>(false);

  getDownloadUrl(id: string): string {
    return this.api.getDownloadUrl(id);
  }

  openInSand3() {
    const it = this.item();
    if (!it) return;

    const uri = `sand3://open?type=${it.type}&id=${it.id}`;
    let appOpened = false;

    const onBlur = () => {
      appOpened = true;
    };
    window.addEventListener('blur', onBlur, { once: true });

    window.location.href = uri;

    setTimeout(() => {
      window.removeEventListener('blur', onBlur);
      if (!appOpened) {
        const dlLink = document.createElement('a');
        dlLink.href = this.getDownloadUrl(it.id);
        dlLink.download = '';
        document.body.appendChild(dlLink);
        dlLink.click();
        document.body.removeChild(dlLink);

        this.showUsageModal.set(true);
      }
    }, 1200);
  }

  closeUsageModal() {
    this.showUsageModal.set(false);
  }

  openEditModal() {
    const it = this.item();
    if (!it) return;
    this.editTitle.set(it.title);
    this.editDescription.set(it.description);
    this.editError.set('');
    this.editingItem.set(true);
  }

  closeEditModal() {
    this.editingItem.set(false);
  }

  saveEdit() {
    const it = this.item();
    if (!it) return;
    const title = this.editTitle().trim();
    if (!title) {
      this.editError.set('Title cannot be empty');
      return;
    }

    this.editLoading.set(true);
    this.editError.set('');

    this.api
      .updateItem(it.id, {
        title,
        description: this.editDescription().trim()
      })
      .subscribe({
        next: (updated) => {
          this.item.update((val) => (val ? { ...val, title: updated.title, description: updated.description } : null));
          this.editLoading.set(false);
          this.closeEditModal();
        },
        error: (err) => {
          this.editLoading.set(false);
          this.editError.set(err.error?.error || 'Failed to update item');
        }
      });
  }

  deleteItem() {
    const it = this.item();
    if (!it) return;
    if (!confirm(`Are you sure you want to permanently delete "${it.title}"?`)) {
      return;
    }

    const deleteObs = this.isAdmin() && !this.isOwner()
      ? this.api.adminDeleteItem(it.id)
      : this.api.deleteItem(it.id);

    deleteObs.subscribe({
      next: () => {
        alert('Item deleted successfully.');
        this.router.navigate(['/workshop']);
      },
      error: (e) => {
        alert('Failed to delete item: ' + (e.error?.error || e.message));
      }
    });
  }

  openReportModal() {
    this.reportReason.set('broken');
    this.reportDetails.set('');
    this.reportSuccess.set('');
    this.reportError.set('');
    this.reportingItem.set(true);
  }

  closeReportModal() {
    this.reportingItem.set(false);
  }

  submitReport() {
    const it = this.item();
    if (!it) return;
    this.reportLoading.set(true);
    this.reportError.set('');
    this.reportSuccess.set('');

    this.api.submitReport(it.id, this.reportReason(), this.reportDetails()).subscribe({
      next: (res) => {
        this.reportLoading.set(false);
        this.reportSuccess.set(res.message || 'Report submitted successfully.');
        setTimeout(() => this.closeReportModal(), 1800);
      },
      error: (e) => {
        this.reportLoading.set(false);
        this.reportError.set(e.error?.error || 'Failed to submit report.');
      }
    });
  }

  parseMeta(metaJson: string): any {
    try {
      return JSON.parse(metaJson);
    } catch {
      return {};
    }
  }

  formatBytes(bytes: number): string {
    if (!bytes || bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }
}
