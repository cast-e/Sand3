import { Component, OnInit, inject, signal, effect } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouterLink, ActivatedRoute, Router } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { ApiService, WorkshopItem } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

@Component({
  selector: 'app-workshop',
  standalone: true,
  imports: [CommonModule, RouterLink, FormsModule],
  templateUrl: './workshop.html',
  styleUrl: './workshop.scss'
})
export class WorkshopComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  items = signal<WorkshopItem[]>([]);
  total = signal<number>(0);
  loading = signal<boolean>(true);

  currentType = signal<string>('all');
  currentSort = signal<string>('popular');
  searchQuery = signal<string>('');
  currentPage = signal<number>(1);

  reportingItem = signal<WorkshopItem | null>(null);
  reportReason = signal<'broken' | 'offensive' | 'spam' | 'other'>('broken');
  reportDetails = signal<string>('');
  reportMessage = signal<string>('');

  editingItem = signal<WorkshopItem | null>(null);
  editTitle = signal<string>('');
  editDescription = signal<string>('');
  editThumbnailData = signal<string>('');
  editLoading = signal<boolean>(false);
  editError = signal<string>('');

  constructor() {
    effect(() => {
      // Whenever currentUser changes (e.g. login or logout), reload items to reflect correct like/favorite states
      const _user = this.auth.currentUser();
      this.loadItems();
    });
  }

  ngOnInit() {
    this.route.queryParams.subscribe((params) => {
      if (params['type']) this.currentType.set(params['type']);
      if (params['sort']) this.currentSort.set(params['sort']);
      if (params['q']) this.searchQuery.set(params['q']);
      if (params['page']) this.currentPage.set(parseInt(params['page'], 10) || 1);
      this.loadItems();
    });
  }

  loadItems() {
    this.loading.set(true);
    this.api
      .getItems(this.currentType(), this.currentSort(), this.searchQuery(), undefined, this.currentPage())
      .subscribe({
        next: (res) => {
          this.items.set(res.items);
          this.total.set(res.pagination.total);
          this.loading.set(false);
        },
        error: (err) => {
          console.error('Failed to load items', err);
          this.loading.set(false);
        }
      });
  }

  setType(type: string) {
    this.currentType.set(type);
    this.currentPage.set(1);
    this.updateQueryParams();
  }

  setSort(sort: string) {
    this.currentSort.set(sort);
    this.currentPage.set(1);
    this.updateQueryParams();
  }

  onSearch() {
    this.currentPage.set(1);
    this.updateQueryParams();
  }

  updateQueryParams() {
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: {
        type: this.currentType() === 'all' ? null : this.currentType(),
        sort: this.currentSort() === 'popular' ? null : this.currentSort(),
        q: this.searchQuery().trim() || null,
        page: this.currentPage() === 1 ? null : this.currentPage()
      },
      queryParamsHandling: 'merge'
    });
  }

  isOwner(item: WorkshopItem): boolean {
    const user = this.auth.currentUser();
    if (!user) return false;
    if (item.user_id && item.user_id === user.id) return true;
    if (!item.user_id && item.author.toLowerCase() === user.username.toLowerCase()) return true;
    return false;
  }

  toggleLike(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    if (!this.auth.currentUser()) {
      alert('Please sign in to like workshop items.');
      return;
    }
    const prevLiked = !!item.is_liked;
    const prevCount = item.likes_count || 0;
    const optimisticLiked = !prevLiked;
    const optimisticCount = Math.max(0, prevCount + (optimisticLiked ? 1 : -1));

    this.items.update((list) =>
      list.map((i) => (i.id === item.id ? { ...i, is_liked: optimisticLiked, likes_count: optimisticCount } : i))
    );

    this.api.toggleLike(item.id).subscribe({
      next: (res) => {
        this.items.update((list) =>
          list.map((i) => (i.id === item.id ? { ...i, is_liked: res.is_liked, likes_count: res.likes_count } : i))
        );
      },
      error: () => {
        this.items.update((list) =>
          list.map((i) => (i.id === item.id ? { ...i, is_liked: prevLiked, likes_count: prevCount } : i))
        );
      }
    });
  }

  toggleFavorite(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    if (!this.auth.currentUser()) {
      alert('Please sign in to favorite workshop items.');
      return;
    }
    const prevFav = !!item.is_favorited;
    const prevCount = item.favorites_count || 0;
    const optimisticFav = !prevFav;
    const optimisticCount = Math.max(0, prevCount + (optimisticFav ? 1 : -1));

    this.items.update((list) =>
      list.map((i) => (i.id === item.id ? { ...i, is_favorited: optimisticFav, favorites_count: optimisticCount } : i))
    );

    this.api.toggleFavorite(item.id).subscribe({
      next: (res) => {
        this.items.update((list) =>
          list.map((i) =>
            i.id === item.id ? { ...i, is_favorited: res.is_favorited, favorites_count: res.favorites_count } : i
          )
        );
      },
      error: () => {
        this.items.update((list) =>
          list.map((i) => (i.id === item.id ? { ...i, is_favorited: prevFav, favorites_count: prevCount } : i))
        );
      }
    });
  }

  openEditModal(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    this.editingItem.set(item);
    this.editTitle.set(item.title);
    this.editDescription.set(item.description);
    this.editThumbnailData.set('');
    this.editError.set('');
  }

  closeEditModal() {
    this.editingItem.set(null);
  }

  onThumbnailSelected(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files[0]) {
      const reader = new FileReader();
      reader.onload = (e: any) => {
        this.editThumbnailData.set(e.target.result);
      };
      reader.readAsDataURL(input.files[0]);
    }
  }

  submitEdit() {
    const item = this.editingItem();
    if (!item) return;

    this.editLoading.set(true);
    this.editError.set('');

    const payload: any = {
      title: this.editTitle().trim(),
      description: this.editDescription().trim()
    };
    if (this.editThumbnailData()) {
      payload.thumbnail_data = this.editThumbnailData();
    }

    this.api.updateItem(item.id, payload).subscribe({
      next: (updated) => {
        this.editLoading.set(false);
        this.items.update((list) => list.map((i) => (i.id === item.id ? { ...i, ...updated } : i)));
        this.closeEditModal();
      },
      error: (err) => {
        this.editLoading.set(false);
        this.editError.set(err.error?.error || 'Failed to update item.');
      }
    });
  }

  deleteItem(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    if (!confirm(`Are you sure you want to delete "${item.title}"? This cannot be undone.`)) {
      return;
    }

    const previousList = this.items();
    const previousTotal = this.total();


    this.items.update((list) => list.filter((i) => i.id !== item.id));
    this.total.update((t) => Math.max(0, t - 1));

    this.api.deleteItem(item.id).subscribe({
      next: () => {

      },
      error: (err) => {

        this.items.set(previousList);
        this.total.set(previousTotal);
        alert(err.error?.error || 'Failed to delete item.');
      }
    });
  }

  openReportModal(item: WorkshopItem, event: Event) {
    event.stopPropagation();
    if (!this.auth.currentUser()) {
      alert('Please sign in to report items.');
      return;
    }
    this.reportingItem.set(item);
    this.reportReason.set('broken');
    this.reportDetails.set('');
    this.reportMessage.set('');
  }

  closeReportModal() {
    this.reportingItem.set(null);
  }

  submitReport() {
    const item = this.reportingItem();
    if (!item) return;

    this.api.submitReport(item.id, this.reportReason(), this.reportDetails()).subscribe({
      next: (res) => {
        this.reportMessage.set(res.message || 'Report submitted successfully.');
        setTimeout(() => this.closeReportModal(), 1500);
      },
      error: () => {
        this.reportMessage.set('Failed to submit report. Please try again.');
      }
    });
  }

  getDownloadUrl(id: string): string {
    return this.api.getDownloadUrl(id);
  }

  parseMeta(metaJson: string): any {
    try {
      return JSON.parse(metaJson);
    } catch {
      return {};
    }
  }

  showUsageModal = signal<boolean>(false);
  currentOpenItem = signal<WorkshopItem | null>(null);

  openInSand3(it: WorkshopItem, event?: Event) {
    if (event) {
      event.stopPropagation();
    }
    this.currentOpenItem.set(it);
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

  getDimensions(item: WorkshopItem): string | null {
    if (item.type === 'set') return null; const meta = this.parseMeta(item.meta_json);
    if (meta.width && meta.height) {
      return `${meta.width}×${meta.height}`;
    }
    return null;
  }
}
